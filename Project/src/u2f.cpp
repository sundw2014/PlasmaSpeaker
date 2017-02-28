extern "C" {
#include "stm32f10x.h"
#define Debug_USART USART1
void USART_printf(USART_TypeDef* USARTx, char *Data, ...);
#define DBG_MSG(format, ...) USART_printf(Debug_USART, "[Debug]%s: " format "\r\n", __func__, ##__VA_ARGS__)
#include "sha256.h"
#include "uECC.h"
#include "time.h"
#include "RawHID.h"
#include <cstring>
#include "eeprom.h"
}

#undef DEBUG
#define DEBUG
#undef DESKTOP_TEST
// #define DESKTOP_TEST

// #define SIMULATE_BUTTON

typedef uint8_t byte;

#define CID_BROADCAST           0xffffffff  // Broadcast channel id

#define TYPE_MASK               0x80  // Frame type mask
#define TYPE_INIT               0x80  // Initial frame identifier
#define TYPE_CONT               0x00  // Continuation frame identifier


#define U2FHID_PING         (TYPE_INIT | 0x01)  // Echo data through local processor only
#define U2FHID_MSG          (TYPE_INIT | 0x03)  // Send U2F message frame
#define U2FHID_LOCK         (TYPE_INIT | 0x04)  // Send lock channel command
#define U2FHID_INIT         (TYPE_INIT | 0x06)  // Channel initialization
#define U2FHID_WINK         (TYPE_INIT | 0x08)  // Send device identification wink
#define U2FHID_ERROR        (TYPE_INIT | 0x3f)  // Error response

// Errors
#define ERR_NONE  0
#define ERR_INVALID_CMD  1
#define ERR_INVALID_PAR  2
#define ERR_INVALID_LEN  3
#define ERR_INVALID_SEQ  4
#define ERR_MSG_TIMEOUT  5
#define ERR_CHANNEL_BUSY  6
#define ERR_LOCK_REQUIRED  10
#define ERR_INVALID_CID  11
#define ERR_OTHER  127

#define U2F_INS_REGISTER  0x01
#define U2F_INS_AUTHENTICATE  0x02
#define U2F_INS_VERSION  0x03


#define STATE_CHANNEL_AVAILABLE 0
#define STATE_CHANNEL_WAIT_PACKET 1
#define STATE_CHANNEL_WAIT_CONT 2
#define STATE_CHANNEL_TIMEOUT 3
#define STATE_LARGE_PACKET 4

#define MAX_TOTAL_PACKET 7609

#define MAX_INITIAL_PACKET 57
#define MAX_CONTINUATION_PACKET 59
#define SET_MSG_LEN(b, v) do { (b)[5] = ((v) >> 8) & 0xff;  (b)[6] = (v) & 0xff; } while(0)


#define U2FHID_IF_VERSION       2  // Current interface implementation version

byte expected_next_packet;
int large_data_len;
int large_data_offset;
byte large_buffer[1024];
byte large_resp_buffer[1024];
byte recv_buffer[64];
byte resp_buffer[64];
byte handle[64];
byte sha256_hash[32];
#define MAX_CHANNEL 4

const char attestation_key[] = "\xf3\xfc\xcc\x0d\x00\xd8\x03\x19\x54\xf9"
	"\x08\x64\xd4\x3c\x24\x7f\x4b\xf5\xf0\x66\x5c\x6b\x50\xcc"
	"\x17\x74\x9a\x27\xd1\xcf\x76\x64";

const char attestation_der[] = "\x30\x82\x01\x3c\x30\x81\xe4\xa0\x03\x02"
	"\x01\x02\x02\x0a\x47\x90\x12\x80\x00\x11\x55\x95\x73\x52"
	"\x30\x0a\x06\x08\x2a\x86\x48\xce\x3d\x04\x03\x02\x30\x17"
	"\x31\x15\x30\x13\x06\x03\x55\x04\x03\x13\x0c\x47\x6e\x75"
	"\x62\x62\x79\x20\x50\x69\x6c\x6f\x74\x30\x1e\x17\x0d\x31"
	"\x32\x30\x38\x31\x34\x31\x38\x32\x39\x33\x32\x5a\x17\x0d"
	"\x31\x33\x30\x38\x31\x34\x31\x38\x32\x39\x33\x32\x5a\x30"
	"\x31\x31\x2f\x30\x2d\x06\x03\x55\x04\x03\x13\x26\x50\x69"
	"\x6c\x6f\x74\x47\x6e\x75\x62\x62\x79\x2d\x30\x2e\x34\x2e"
	"\x31\x2d\x34\x37\x39\x30\x31\x32\x38\x30\x30\x30\x31\x31"
	"\x35\x35\x39\x35\x37\x33\x35\x32\x30\x59\x30\x13\x06\x07"
	"\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48\xce\x3d"
	"\x03\x01\x07\x03\x42\x00\x04\x8d\x61\x7e\x65\xc9\x50\x8e"
	"\x64\xbc\xc5\x67\x3a\xc8\x2a\x67\x99\xda\x3c\x14\x46\x68"
	"\x2c\x25\x8c\x46\x3f\xff\xdf\x58\xdf\xd2\xfa\x3e\x6c\x37"
	"\x8b\x53\xd7\x95\xc4\xa4\xdf\xfb\x41\x99\xed\xd7\x86\x2f"
	"\x23\xab\xaf\x02\x03\xb4\xb8\x91\x1b\xa0\x56\x99\x94\xe1"
	"\x01\x30\x0a\x06\x08\x2a\x86\x48\xce\x3d\x04\x03\x02\x03"
	"\x47\x00\x30\x44\x02\x20\x60\xcd\xb6\x06\x1e\x9c\x22\x26"
	"\x2d\x1a\xac\x1d\x96\xd8\xc7\x08\x29\xb2\x36\x65\x31\xdd"
	"\xa2\x68\x83\x2c\xb8\x36\xbc\xd3\x0d\xfa\x02\x20\x63\x1b"
	"\x14\x59\xf0\x9e\x63\x30\x05\x57\x22\xc8\xd8\x9b\x7f\x48"
	"\x88\x3b\x90\x89\xb8\x8d\x60\xd1\xd9\x79\x59\x02\xb3\x04"
	"\x10\xdf";

//key handle: (private key + app parameter) ^ this array
const char handlekey[] = "SKYWORKS-innovation201605-stm32--";

const struct uECC_Curve_t * curve = uECC_secp256r1(); //P-256
uint8_t private_k[36]; //32
uint8_t public_k[68]; //64

typedef struct _ch_state {
	int cid;
	byte state;
	int last_millis;
}ch_state;

ch_state channel_states[MAX_CHANNEL];

#ifdef DESKTOP_TEST
extern int RNG(uint8_t *dest, unsigned size);
#else
extern "C" {

	static int RNG(uint8_t *dest, unsigned size) {
		// Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of
		// random noise). This can take a long time to generate random data if the result of analogRead(0)
		// doesn't change very frequently.
		while (size) {
			uint8_t val = 0;
			for (unsigned i = 0; i < 8; ++i) {
				int init = millis();//analogRead(0);
				int count = 0;
				while (millis() == init) {
					++count;
				}

				if (count == 0) {
					val = (val << 1) | (init & 0x01);
				} else {
					val = (val << 1) | (count & 0x01);
				}
			}
			*dest = val;
			++dest;
			--size;
		}
		// NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
		return 1;
	}

}  // extern "C"
#endif

#ifndef DESKTOP_TEST
#ifdef DEBUG
char itoh(uint8_t i)
{
	if(i<10)
	{
		return i+'0';
	}
	else{
		return (i-10)+'A';
	}
}
extern "C"{

void dump_hex(byte *buffer, int len)
{
	char hex[200];
	len = len>64?64:len;
	int i = 0;
	for ( ; i < len; i++) {
		hex[3*i] = itoh((buffer[i]&0xF0)>>4);
		hex[3*i+1] = itoh(buffer[i]&0x0F);
		hex[3*i+2] = ' ';
	}
	hex[3*i] = '\0';
	DBG_MSG("%s",hex);
}
}
#endif
#endif

#define TIMEOUT_VALUE 1000

typedef struct SHA256_HashContext {
    uECC_HashContext uECC;
    SHA256_CTX ctx;
} SHA256_HashContext;

void init_SHA256(uECC_HashContext *base) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    sha256_init(&context->ctx);
}
void update_SHA256(uECC_HashContext *base,
                   const uint8_t *message,
                   unsigned message_size) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    sha256_update(&context->ctx, message, message_size);
}
void finish_SHA256(uECC_HashContext *base, uint8_t *hash_result) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    sha256_final(&context->ctx, hash_result);
}

extern "C" {
void u2f_init() {
	uECC_set_rng(&RNG);
}
}
void cleanup_timeout()
{
	int i;
	for (i = 0;  i < MAX_CHANNEL; i++) {
		//free channel that is inactive
		ch_state &c = channel_states[i];
		int m = millis();
		if (c.state != STATE_CHANNEL_AVAILABLE) {
			if ((m - c.last_millis) > TIMEOUT_VALUE) {
				c.state = STATE_CHANNEL_AVAILABLE;
			}
		}
	}
}

int allocate_new_channel()
{
	int i;
	//alloace new channel_id
	int channel_id = 1;

	do {
		bool found = false;
		for (i = 0;  i < MAX_CHANNEL; i++) {
			if (channel_states[i].state != STATE_CHANNEL_AVAILABLE) {
				if (channel_states[i].cid == channel_id) {
					found = true;
					channel_id++;
					break;
				}
			}
		}
		if (!found)
			break;
	} while (true);
	return channel_id;
}

int allocate_channel(int channel_id)
{
	int i;
	if (channel_id==0) {
		channel_id =  allocate_new_channel();
	}

	bool has_free_slots = false;
	for (i = 0;  i < MAX_CHANNEL; i++) {
		if (channel_states[i].state == STATE_CHANNEL_AVAILABLE) {
			has_free_slots = true;
			break;
		}
	}
	if (!has_free_slots)
		cleanup_timeout();

	for (i = 0;  i < MAX_CHANNEL; i++) {
		ch_state &c = channel_states[i];
		if (c.state == STATE_CHANNEL_AVAILABLE) {
			c.cid = channel_id;
			c.state = STATE_CHANNEL_WAIT_PACKET;
			c.last_millis = millis();
			return channel_id;
		}
	}
	return 0;
}

int initResponse(byte *buffer)
{
#ifdef DEBUG
	DBG_MSG("INIT RESPONSE");
#endif
	int cid = *(int*)buffer;
#ifdef DEBUG
	// Serial.println(cid, HEX);
#endif
	int len = buffer[5] << 8 | buffer[6];
	int i;
	memcpy(resp_buffer, buffer, 5);
	SET_MSG_LEN(resp_buffer, 17);
	memcpy(resp_buffer + 7, buffer + 7, len); //nonce
	i = 7 + len;
	if (cid==-1) {
		cid = allocate_channel(0);
	} else {
#ifdef DEBUG
		DBG_MSG("using existing CID");
#endif
		allocate_channel(cid);
	}
	memcpy(resp_buffer + i, &cid, 4);
	i += 4;
	resp_buffer[i++] = U2FHID_IF_VERSION;
	resp_buffer[i++] = 1; //major
	resp_buffer[i++] = 0;
	resp_buffer[i++] = 1; //build
	//resp_buffer[i++] = CAPABILITY_WINK; //capabilities
	resp_buffer[i++] = 0; //capabilities
#ifdef DEBUG
	DBG_MSG("SENT RESPONSE 1");
#endif
	RawHID_send(resp_buffer, 64);
#ifdef DEBUG
	// Serial.println(cid, HEX);
#endif
	return cid;
}


void errorResponse(byte *buffer, int code)
{
        memcpy(resp_buffer, buffer, 4);
        resp_buffer[4] = U2FHID_ERROR;
        SET_MSG_LEN(resp_buffer, 1);
        resp_buffer[7] = code & 0xff;
#ifdef DEBUG
	// Serial.print("SENT RESPONSE error:");
	// DBG_MSG(code);
#endif
	RawHID_send(resp_buffer, 64);
}


//find channel index and update last access
int find_channel_index(int channel_id)
{
	int i;

	for (i = 0;  i < MAX_CHANNEL; i++) {
		if (channel_states[i].cid==channel_id) {
			channel_states[i].last_millis = millis();
			return i;
		}
	}

	return -1;
}

#define IS_CONTINUATION_PACKET(x) ( (x) < 0x80)
#define IS_NOT_CONTINUATION_PACKET(x) ( (x) >= 0x80)

#define SW_NO_ERROR                       0x9000
#define SW_CONDITIONS_NOT_SATISFIED       0x6985
#define SW_WRONG_DATA                     0x6A80
#define SW_WRONG_LENGTH                     0x6700
#define SW_INS_NOT_SUPPORTED 0x6D00
#define SW_CLA_NOT_SUPPORTED 0x6E00


#define APPEND_SW(x, v1, v2) do { (*x++)=v1; (*x++)=v2;} while (0)
#define APPEND_SW_NO_ERROR(x) do { (*x++)=0x90; (*x++)=0x00;} while (0)



void respondErrorPDU(byte *buffer, int err)
{
	SET_MSG_LEN(buffer, 2); //len("") + 2 byte SW
	byte *datapart = buffer + 7;
	APPEND_SW(datapart, (err >> 8) & 0xff, err & 0xff);
	RawHID_send(buffer, 64);
}

void sendLargeResponse(byte *request, int len)
{
#ifdef DEBUG
	DBG_MSG("Sending large response,len=%d",len);
	// DBG_MSG(len);
	dump_hex(large_resp_buffer,len);
#endif
	memcpy(resp_buffer, request, 4); //copy cid
	resp_buffer[4] = U2FHID_MSG;
	int r = len;
	if (r>MAX_INITIAL_PACKET) {
		r = MAX_INITIAL_PACKET;
	}

	SET_MSG_LEN(resp_buffer, len);
	memcpy(resp_buffer + 7, large_resp_buffer, r);

	RawHID_send(resp_buffer, 64);
	// delayMicroseconds(2500);
	len -= r;
	byte p = 0;
	int offset = MAX_INITIAL_PACKET;
	while (len > 0) {
		//memcpy(resp_buffer, request, 4); //copy cid, doesn't need to recopy
		resp_buffer[4] = p++;
		memcpy(resp_buffer + 5, large_resp_buffer + offset, MAX_CONTINUATION_PACKET);
		RawHID_send(resp_buffer, 64);
		len-= MAX_CONTINUATION_PACKET;
		offset += MAX_CONTINUATION_PACKET;
		// delayMicroseconds(2500);
	}
}

// static int _counter;
int getCounter() {
	uint16_t eeAddress = 0; //EEPROM address to start reading from
	uint16_t counter;
	EE_ReadVariable( eeAddress, &counter );
	DBG_MSG("counter=%d",counter);
	return counter;
}

void setCounter(int counter)
{
	DBG_MSG("counter=%d",counter);
	uint16_t eeAddress = 0; //EEPROM address to start reading from
	// EEPROM.put( eeAddress, counter );
	EE_WriteVariable(eeAddress, counter);
	// _counter=counter;
}

#ifdef SIMULATE_BUTTON
//for now just simulate this
int button_pressed = 0;
#endif

void processMessage(byte *buffer)
{
	int len = buffer[5] << 8 | buffer[6];
#ifdef DEBUG
	DBG_MSG("Got message, len=%d", len);
#endif
	byte *message = buffer + 7;
#ifdef DEBUG
	dump_hex(buffer+7, len);
#endif
	//todo: check CLA = 0
	byte CLA = message[0];

	if (CLA!=0) {
		respondErrorPDU(buffer, SW_CLA_NOT_SUPPORTED);
		return;
	}

	byte INS = message[1];
	byte P1 = message[2];
	//byte P2 = message[3];
	int reqlength = (message[4] << 16) | (message[5] << 8) | message[6];

	switch (INS) {
	case U2F_INS_REGISTER:
		{
			if (reqlength!=64) {
				respondErrorPDU(buffer, SW_WRONG_LENGTH);
				return;
			}

#ifdef SIMULATE_BUTTON
			if (!button_pressed) {
				respondErrorPDU(buffer, SW_CONDITIONS_NOT_SATISFIED);
				button_pressed = 1;
				return;
			}
#endif

			byte *datapart = message + 7;
			byte *challenge_parameter = datapart;
			byte *application_parameter = datapart+32;

			memset(public_k, 0, sizeof(public_k));
			memset(private_k, 0, sizeof(private_k));
			uECC_make_key(public_k + 1, private_k, curve); //so we ca insert 0x04
			public_k[0] = 0x04;
#ifdef DEBUG
			DBG_MSG("Public K");
			dump_hex(public_k,sizeof(public_k));
			DBG_MSG("Private K");
			dump_hex(private_k,sizeof(private_k));
#endif
			//construct hash

			memcpy(handle, application_parameter, 32);
			memcpy(handle+32, private_k, 32);
			for (int i =0; i < 64; i++) {
				handle[i] ^= handlekey[i%(sizeof(handlekey)-1)];
			}

			SHA256_CTX ctx;
			sha256_init(&ctx);
			large_resp_buffer[0] = 0x00;
			sha256_update(&ctx, large_resp_buffer, 1);
#ifdef DEBUG
			DBG_MSG("App Parameter:");
			dump_hex(application_parameter,32);
#endif
			sha256_update(&ctx, application_parameter, 32);
#ifdef DEBUG
			DBG_MSG("Chal Parameter:");
			dump_hex(challenge_parameter,32);
#endif
			sha256_update(&ctx, challenge_parameter, 32);
#ifdef DEBUG
			DBG_MSG("Handle Parameter:");
			dump_hex(handle,64);
#endif
			sha256_update(&ctx, handle, 64);
			sha256_update(&ctx, public_k, 65);
#ifdef DEBUG
			DBG_MSG("Public key:");
			dump_hex(public_k,65);
#endif
			sha256_final(&ctx, sha256_hash);
#ifdef DEBUG
			DBG_MSG("Hash:");
			dump_hex(sha256_hash,32);
#endif

			uint8_t *signature = resp_buffer; //temporary

			uint8_t tmp[32 + 32 + 64];
			SHA256_HashContext ectx = {{&init_SHA256, &update_SHA256, &finish_SHA256, 64, 32, tmp}};


			uECC_sign_deterministic((uint8_t *)attestation_key,
						sha256_hash,
						32,
						&ectx.uECC,
						signature,
						curve);

			int len = 0;
			large_resp_buffer[len++] = 0x05;
			memcpy(large_resp_buffer + len, public_k, 65);
			len+=65;
			large_resp_buffer[len++] = 64; //length of handle
			memcpy(large_resp_buffer+len, handle, 64);
			len += 64;
			memcpy(large_resp_buffer+len, attestation_der, sizeof(attestation_der));
			len += sizeof(attestation_der)-1;
			//convert signature format
			//http://bitcoin.stackexchange.com/questions/12554/why-the-signature-is-always-65-13232-bytes-long
			large_resp_buffer[len++] = 0x30; //header: compound structure
			uint8_t *total_len = &large_resp_buffer[len];
			large_resp_buffer[len++] = 0x44; //total length (32 + 32 + 2 + 2)
			large_resp_buffer[len++] = 0x02;  //header: integer

			if (signature[0]>0x7f) {
			   	large_resp_buffer[len++] = 33;  //33 byte
				large_resp_buffer[len++] = 0;
				(*total_len)++; //update total length
			}  else {
				large_resp_buffer[len++] = 32;  //32 byte
			}

			memcpy(large_resp_buffer+len, signature, 32); //R value
			len +=32;
			large_resp_buffer[len++] = 0x02;  //header: integer

			if (signature[32]>0x7f) {
				large_resp_buffer[len++] = 33;  //32 byte
				large_resp_buffer[len++] = 0;
				(*total_len)++;	//update total length
			} else {
				large_resp_buffer[len++] = 32;  //32 byte
			}

			memcpy(large_resp_buffer+len, signature+32, 32); //R value
			len +=32;

			byte *last = large_resp_buffer+len;
			APPEND_SW_NO_ERROR(last);
			len += 2;
#ifdef SIMULATE_BUTTON
			button_pressed = 0;
#endif
			sendLargeResponse(buffer, len);
		}

		break;
	case U2F_INS_AUTHENTICATE:
		{
DBG_MSG("U2F_INS_AUTHENTICATE");
			//minimum is 64 + 1 + 64
			if (reqlength!=(64+1+64)) {
				respondErrorPDU(buffer, SW_WRONG_LENGTH);
				return;
			}

			byte *datapart = message + 7;
			byte *challenge_parameter = datapart;
			byte *application_parameter = datapart+32;
			byte handle_len = datapart[64];
			byte *client_handle = datapart+65;

			if (handle_len!=64) {
				//not from this device
				respondErrorPDU(buffer, SW_WRONG_DATA);
				return;
			}
#ifdef SIMULATE_BUTTON
			if (!button_pressed) {
				respondErrorPDU(buffer, SW_CONDITIONS_NOT_SATISFIED);
				button_pressed = 1;
				return;
			}
#endif

			memcpy(handle, client_handle, 64);
			for (int i =0; i < 64; i++) {
				handle[i] ^= handlekey[i%(sizeof(handlekey)-1)];
			}
			uint8_t *key = handle + 32;

			if (memcmp(handle, application_parameter, 32)!=0) {
				//this handle is not from us
				respondErrorPDU(buffer, SW_WRONG_DATA);
				return;
			}

			if (P1==0x07) { //check-only
				DBG_MSG("check-only");
				respondErrorPDU(buffer, SW_CONDITIONS_NOT_SATISFIED);
			} else if (P1==0x03) { //enforce-user-presence-and-sign
				DBG_MSG("enforce");
				int counter = getCounter();
				SHA256_CTX ctx;
				sha256_init(&ctx);
				sha256_update(&ctx, application_parameter, 32);
				large_resp_buffer[0] = 0x01; // user_presence

				int ctr = ((counter>>24)&0xff) | // move byte 3 to byte 0
					((counter<<8)&0xff0000) | // move byte 1 to byte 2
					((counter>>8)&0xff00) | // move byte 2 to byte 1
					((counter<<24)&0xff000000); // byte 0 to byte 3

				memcpy(large_resp_buffer + 1, &ctr, 4);

				sha256_update(&ctx, large_resp_buffer, 5); //user presence + ctr

				sha256_update(&ctx, challenge_parameter, 32);
				sha256_final(&ctx, sha256_hash);

				uint8_t *signature = resp_buffer; //temporary

				uint8_t tmp[32 + 32 + 64];
				SHA256_HashContext ectx = {{&init_SHA256, &update_SHA256, &finish_SHA256, 64, 32, tmp}};

				uECC_sign_deterministic((uint8_t *)key,
							sha256_hash,
							32,
							&ectx.uECC,
							signature,
							curve);

				int len = 5;

				//convert signature format
				//http://bitcoin.stackexchange.com/questions/12554/why-the-signature-is-always-65-13232-bytes-long
				large_resp_buffer[len++] = 0x30; //header: compound structure
				uint8_t *total_len = &large_resp_buffer[len];
				large_resp_buffer[len++] = 0x44; //total length (32 + 32 + 2 + 2)
				large_resp_buffer[len++] = 0x02;  //header: integer

				if (signature[0]>0x7f) {
			   	   large_resp_buffer[len++] = 33;  //33 byte
				   large_resp_buffer[len++] = 0;
				   (*total_len)++; //update total length
				} else {
				   large_resp_buffer[len++] = 32;  //32 byte
				}

				memcpy(large_resp_buffer+len, signature, 32); //R value
				len +=32;
				large_resp_buffer[len++] = 0x02;  //header: integer

				if (signature[32]>0x7f) {
				    large_resp_buffer[len++] = 33;  //32 byte
				    large_resp_buffer[len++] = 0;
				    (*total_len)++;	//update total length
				} else {
				    large_resp_buffer[len++] = 32;  //32 byte
				}

				memcpy(large_resp_buffer+len, signature+32, 32); //R value
				len +=32;
				byte *last = large_resp_buffer+len;
				APPEND_SW_NO_ERROR(last);
				len += 2;
#ifdef DEBUG
				// // Serial.print("Len to send ");
				// // Serial.println(len);
#endif
#ifdef SIMULATE_BUTTON
				button_pressed = 0;
#endif
				sendLargeResponse(buffer, len);

				setCounter(counter+1);
			} else {
				//return error
			}
		}
		break;
	case U2F_INS_VERSION:
		{
			if (reqlength!=0) {
				respondErrorPDU(buffer, SW_WRONG_LENGTH);
				return;
			}
			//reuse input buffer for sending
			SET_MSG_LEN(buffer, 8); //len("U2F_V2") + 2 byte SW
			byte *datapart = buffer + 7;
			memcpy(datapart, "U2F_V2", 6);
			datapart += 6;
			APPEND_SW_NO_ERROR(datapart);
			RawHID_send(buffer, 64);
		}
		break;
	default:
		{
			respondErrorPDU(buffer, SW_INS_NOT_SUPPORTED);
		}
		;
	}

}

void processPacket(byte *buffer)
{
#ifdef DEBUG
	// // Serial.print("Process CMD ");
#endif
	unsigned char cmd = buffer[4]; //cmd or continuation
#ifdef DEBUG
	// // Serial.println((int)cmd, HEX);
#endif

	int len = buffer[5] << 8 | buffer[6];
	if (cmd > U2FHID_INIT || cmd==U2FHID_LOCK) {
		errorResponse(recv_buffer, ERR_INVALID_CMD);
		return;
	}
	if (cmd==U2FHID_PING) {
		if (len <= MAX_INITIAL_PACKET) {
#ifdef DEBUG
			// Serial.println("Sending ping response");
#endif
			RawHID_send(buffer, 64);
		} else {
			//large packet
			//send first one
#ifdef DEBUG
			DBG_MSG("SENT RESPONSE 3");
#endif
			RawHID_send(buffer, 64);
			// delayMicroseconds(2500);
			len -= MAX_INITIAL_PACKET;
			byte p = 0;
			int offset = 7 + MAX_INITIAL_PACKET;
			while (len > 0) {
				memcpy(resp_buffer, buffer, 4); //copy cid
				resp_buffer[4] = p++;
				memcpy(resp_buffer + 5, buffer + offset, MAX_CONTINUATION_PACKET);
				RawHID_send(resp_buffer, 64);
				len-= MAX_CONTINUATION_PACKET;
				offset += MAX_CONTINUATION_PACKET;
				// delayMicroseconds(2500);
			}
#ifdef DEBUG
			DBG_MSG("Sending large ping response");
#endif
		}
	}
	if (cmd==U2FHID_MSG) {
		processMessage(buffer);
	}

}

void setOtherTimeout()
{
	//we can process the data
	//but if we find another channel is waiting for continuation, we set it as timeout
	for (int i = 0; i < MAX_CHANNEL; i++) {
		if (channel_states[i].state==STATE_CHANNEL_WAIT_CONT) {
#ifdef DEBUG
			DBG_MSG("Set other timeout");
#endif
			channel_states[i].state= STATE_CHANNEL_TIMEOUT;
		}
	}

}

int cont_start = 0;

extern "C" {
void u2f_loop() {
	int n;
// DBG_MSG("alive");
	n = RawHID_recv(recv_buffer, PACK_SIZE);

	if (n > 0) {
		DBG_MSG("new data");

#ifdef DEBUG
#ifndef DESKTOP_TEST
       dump_hex(recv_buffer, n);
#endif
		DBG_MSG("Received packet, CID: ");
#endif
		//int cid = *(int*)recv_buffer;
		int cid; //handle strict-aliasing warning
		memcpy(&cid, recv_buffer, sizeof(cid));
#ifdef DEBUG
		DBG_MSG("cid:%d",cid);
#endif
		if (cid==0) {
			errorResponse(recv_buffer, ERR_INVALID_CID);
			return;
		}

		unsigned char cmd_or_cont = recv_buffer[4]; //cmd or continuation


		int len = (recv_buffer[5]) << 8 | recv_buffer[6];

#ifdef DEBUG
		if (IS_NOT_CONTINUATION_PACKET(cmd_or_cont)) {
			// Serial.print(F("LEN "));
			// Serial.println((int)len);
		}
#endif


		//don't care about cid
		if (cmd_or_cont==U2FHID_INIT) {
			setOtherTimeout();
			cid = initResponse(recv_buffer);
			int cidx = find_channel_index(cid);
			channel_states[cidx].state= STATE_CHANNEL_WAIT_PACKET;
			return;
		}

		if (cid==-1) {
			errorResponse(recv_buffer, ERR_INVALID_CID);
			return;
		}

		int cidx = find_channel_index(cid);

		if (cidx==-1) {
#ifdef DEBUG
			DBG_MSG("allocating new CID");
#endif
			allocate_channel(cid);
			cidx = find_channel_index(cid);
			if (cidx==-1) {
				errorResponse(recv_buffer, ERR_INVALID_CID);
				return;
			}

		}

		if (IS_NOT_CONTINUATION_PACKET(cmd_or_cont)) {

			if (len > MAX_TOTAL_PACKET) {
				errorResponse(recv_buffer, ERR_INVALID_LEN); //invalid length
				return;
			}

			if (len > MAX_INITIAL_PACKET) {
				//if another channel is waiting for continuation, we respond with busy
				for (int i = 0; i < MAX_CHANNEL; i++) {
					if (channel_states[i].state==STATE_CHANNEL_WAIT_CONT) {
						if (i==cidx) {
							errorResponse(recv_buffer, ERR_INVALID_SEQ); //invalid sequence
							channel_states[i].state= STATE_CHANNEL_WAIT_PACKET;
						} else {
							errorResponse(recv_buffer, ERR_CHANNEL_BUSY);
							return;
						}

						return;
					}
				}
				//no other channel is waiting
				channel_states[cidx].state=STATE_CHANNEL_WAIT_CONT;
				cont_start = millis();
				memcpy(large_buffer, recv_buffer, 64);
				large_data_len = len;
				large_data_offset = MAX_INITIAL_PACKET;
				expected_next_packet = 0;
				return;
			}

			setOtherTimeout();
			processPacket(recv_buffer);
			channel_states[cidx].state= STATE_CHANNEL_WAIT_PACKET;
		} else {
DBG_MSG("continuation packet");
			if (channel_states[cidx].state!=STATE_CHANNEL_WAIT_CONT) {
#ifdef DEBUG
				DBG_MSG("ignoring stray packet");
				// Serial.println(cid, HEX);
#endif
				return;
			}

			//this is a continuation
			if (cmd_or_cont != expected_next_packet) {
				DBG_MSG("cmd_or_cont:%d != expected_next_packet:%d", cmd_or_cont, expected_next_packet);
				errorResponse(recv_buffer, ERR_INVALID_SEQ); //invalid sequence
				channel_states[cidx].state= STATE_CHANNEL_WAIT_PACKET;
				return;
			} else {

				memcpy(large_buffer + large_data_offset + 7, recv_buffer + 5, MAX_CONTINUATION_PACKET);
				large_data_offset += MAX_CONTINUATION_PACKET;

				if (large_data_offset < large_data_len) {
					expected_next_packet++;
#ifdef DEBUG
					DBG_MSG("Expecting next cont");
#endif
					return;
				}
#ifdef DEBUG
				DBG_MSG("Completed");
#endif
				channel_states[cidx].state= STATE_CHANNEL_WAIT_PACKET;
				processPacket(large_buffer);
				return;
			}
		}
	} else {

		for (int i = 0; i < MAX_CHANNEL; i++) {
			if (channel_states[i].state==STATE_CHANNEL_TIMEOUT) {
#ifdef DEBUG
				DBG_MSG("send timeout");
				// Serial.println(channel_states[i].cid, HEX);
#endif
				memcpy(recv_buffer, &channel_states[i].cid, 4);
				errorResponse(recv_buffer, ERR_MSG_TIMEOUT);
				channel_states[i].state= STATE_CHANNEL_WAIT_PACKET;

			}
			if (channel_states[i].state==STATE_CHANNEL_WAIT_CONT) {

				int now = millis();
				if ((now - channel_states[i].last_millis)>500) {
#ifdef DEBUG
					DBG_MSG("SET timeout");
#endif
					channel_states[i].state=STATE_CHANNEL_TIMEOUT;
				}
			}
		}
	}
}
}//extern "C"
