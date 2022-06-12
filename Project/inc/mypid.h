void IncPIDInit_x(void);
int IncPIDCalc_x(int NextPoint);
void IncPIDInit_y(void);
int IncPIDCalc_y(int NextPoint);
typedef struct 
{
        int targetValue;
        float Kp;
        float Ki;
        float Kd;
        float integrationError;
        int lastError;
} PID;
