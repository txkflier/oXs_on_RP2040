#include "pico/stdlib.h"
#include "gyro.h"
#include "param.h"
#include "tools.h"
#include "stdlib.h"
#include "mpu.h"
#include "sbus_in.h"
#include "sbus_out_pwm.h"
#include "crsf_in.h"
#include "hardware/watchdog.h"

// to do : manage the ability to set the gyro in different positions
// this has probably an impact on mpu too!!!!!

extern CONFIG config;
extern MPU mpu;
extern uint16_t rcChannelsUs[16];  // Rc channels values provided by the receiver in Us units 
uint16_t rcChannelsUsCorr[16];  // Rc channels values with gyro corrections applied (in Us)

extern int16_t gyroX; // data provided by mpu, sent to core0 and with some filtering
extern int16_t gyroY;
extern int16_t gyroZ;

extern bool configIsValid;
extern uint8_t ledState;

struct gyroMixer_t gyroMixer ; // contains the parameters provided by the learning process for each of the 16 Rc channel

bool gyroIsInstalled = false;  // becomes true when config.gyroChanControl is defined (not 255) and MPU6050 installed

/*
void initGyroMixer(){
    // this is a temprary solution to get gyro mixer without having yet the learning process (and the saving process)
    
    #define AIL1_CHANNEL 1  // 1 means channel 2  //here the ID of the output channels where compensation apply
    #define AIL2_CHANNEL 1
    #define ELV1_Channel 2  
    #define ELV2_Channel 3
    #define RUD_Channel 4
    for (uint8_t i=0; i<16 ; i++){ // set all mixer as unsued (for gyro)
        gyroMixer.used[i] = false;
        gyroMixer.neutralUs[i] = 1500 ;
        gyroMixer.minUs[i] = 1000 ;
        gyroMixer.maxUs[i] = 2000 ;
        gyroMixer.rateRollLeftUs[i] = 0 ; // 0 means that when stick is at the end pos, this output RC channel is not impacted 
        gyroMixer.rateRollRightUs[i] = 0 ; 
        gyroMixer.ratePitchUpUs[i] = 0 ;  
        gyroMixer.ratePitchDownUs[i] = 0 ;
        gyroMixer.rateYawLeftUs[i] = 0  ;
        gyroMixer.rateYawRightUs[i] = 0 ;   
    }
    // then fill the parameters with your value // normally this should be uploaded from flash.
    gyroMixer.isCalibrated = true ;
    uint8_t i;
    i = AIL1_CHANNEL;    
    gyroMixer.used[i] = true ;
    gyroMixer.neutralUs[i] = 1500;
    gyroMixer.minUs[i] = 1000 ;
    gyroMixer.maxUs[i] = 2000 ;
    gyroMixer.rateRollLeftUs[i] = 400 ; // 300 means that when stick Ail (for Roll) is in the Left corner, RC channel (here AIL1_CHANNEL) is changed by 300 usec 
    gyroMixer.rateRollRightUs[i] = -400 ; // here for stick Ail Right
    // in this set up only AIL_CHANNEL get gyro corrections and only on roll axis ; all other values are the default
    
    // add here other setup for other output Rc channels    
    // e.g. for a second Ail
    //i = AIL2_CHANNEL;    
    //gyroMixer[i].used = true ;
    //gyroMixer.used[i] = true ;
    //gyroMixer.neutralUs[i] = 1700;
    //gyroMixer.minUs[i] = 1000 ;
    //gyroMixer.maxUs[i] = 2000 ;
    //gyroMixer.rateRollLeftUs[i] = 500 ; // 500 means that when stick Ail (for Roll) is in the Left corner, RC channel (here AIL1_CHANNEL) is changed by 500 usec 
    //gyroMixer.rateRollRightUs[i] = -500 ; // here for stick Ail Right
    
    // for e.g. a V stab you have to define values for 2 servos ELV1 and ELV2 and for each 4 rates.
    
}

// temporary solution waiting to allow changes in usb commands
void initGyroConfig(){
    #define GYRO_CHANNEL_CONTROL 8  // 0 means channel 1,  so 8 means channel 9 
    #define GYRO_CHAN_AIL 9 // 0 means channel 1
    #define GYRO_CHAN_ELV 10 // 0 means channel 1
    #define GYRO_CHAN_RUD 11 // 0 means channel 1
     
    
    #define IDX_AIL 0
    #define IDX_ELV 1
    #define IDX_RUD 2
    // this is a temporaty function to define the gyro parameters waiting to be able to fill them with usb command
    config.gyroChanControl = GYRO_CHANNEL_CONTROL; // Rc channel used to say if gyro is implemented or not and to select the mode and the general gain. Value must be in range 1/16 or 255 (no gyro)
    config.gyroChan[IDX_AIL] = GYRO_CHAN_AIL ;    // Rc channel used to transmit original Ail, Elv, Rud stick position ; Value must be in range 1/16 when gyroControlChannel is not 255
    config.gyroChan[IDX_ELV] = GYRO_CHAN_ELV ;    // Rc channel used to transmit original Ail, Elv, Rud stick position ; Value must be in range 1/16 when gyroControlChannel is not 255
    config.gyroChan[IDX_RUD] = GYRO_CHAN_RUD ;    // Rc channel used to transmit original Ail, Elv, Rud stick position ; Value must be in range 1/16 when gyroControlChannel is not 255
    for (uint8_t i = 0; i<3; i++){
        config.pid_param_rate.kp[i] = 500; // default PID param used by flightstab
        config.pid_param_rate.ki[i] = 0; // default PID param
        config.pid_param_rate.kd[i] = 500; // default PID param
        config.pid_param_hold.kp[i] = 500; // default PID param
        config.pid_param_hold.ki[i] = 500; // default PID param
        config.pid_param_hold.kd[i] = 500; // default PID param
    }
    config.pid_param_rate.output_shift = 8; // default used by flightstab
    config.pid_param_hold.output_shift = 8;
    config.vr_gain[IDX_AIL] = 127;          // store the gain per axis, max value is 128 (to combine with global gain provided by gyroChanControl)
    config.vr_gain[IDX_ELV] = 127;          // store the gain per axis, max value is 128 (to combine with global gain provided by gyroChanControl)
    config.vr_gain[IDX_RUD] = 127;          // store the gain per axis, max value is 128 (to combine with global gain provided by gyroChanControl)
    config.stick_gain_throw = STICK_GAIN_THROW_FULL ;  //STICK_GAIN_THROW_FULL=1, STICK_GAIN_THROW_HALF=2, STICK_GAIN_THROW_QUARTER=3 
} 
*/

/***************************************************************************************************************
 * PID
 ***************************************************************************************************************/

#define PID_PERIOD 10000
// relative exponential weights kp:ki:kd
#define PID_KP_SHIFT 3 
#define PID_KI_SHIFT 6
#define PID_KD_SHIFT 2

struct _pid_state {
  // setpoint, input [-8192, 8191]
  int16_t setpoint[3];
  int16_t input[3];
  int16_t last_err[3];
  int32_t sum_err[3];
  int32_t i_limit[3];
  int16_t output[3];
};

struct _pid_state pid_state; // pid engine state

int32_t constrain(int32_t x , int32_t min, int32_t max ){
    if (x < min) return min;
    if (x > max) return max;
    return x;
}


void compute_pid(struct _pid_state *ppid_state, struct _pid_param *ppid_param) 
{
  for (int8_t i=0; i<3; i++) {
    int32_t err, sum_err, diff_err, pterm, iterm, dterm;
    err = ppid_state->input[i] - ppid_state->setpoint[i];
    // accumulate the error up to an i_limit threshold
    sum_err = ppid_state->sum_err[i] = constrain(ppid_state->sum_err[i] + err, -ppid_state->i_limit[i], ppid_state->i_limit[i]);
    diff_err = err - ppid_state->last_err[i]; // difference the error
    ppid_state->last_err[i] = err;    
        
    pterm = (ppid_param->kp[i] * err) >> PID_KP_SHIFT;
    iterm = (ppid_param->ki[i] * sum_err) >> PID_KI_SHIFT;
    dterm = (ppid_param->kd[i] * diff_err) >> PID_KD_SHIFT;
    ppid_state->output[i] = (pterm + iterm + dterm) >> ppid_param->output_shift;

#if defined(SERIAL_DEBUG) && 0
    if (i == 2) {
      Serial.print(ppid_state->input[i]); Serial.print('\t');
      Serial.print(err); Serial.print('\t');
      Serial.print(diff_err); Serial.print('\t');
      Serial.print(pterm); Serial.print('\t');
      Serial.print(iterm); Serial.print('\t');
      Serial.print(ppid_state->sum_err[i]); Serial.print('\t');
      Serial.print(dterm); Serial.print('\t');
      Serial.println(ppid_state->output[i]);
    }
#endif
  }
}


// ----------- compensation from gyro ----------------------
uint32_t last_pid_time = 0;

  
enum STAB_MODE stabMode = STAB_RATE;
// rx
#define RX_GAIN_HYSTERESIS 25
#define RX_MODE_HYSTERESIS 25
#define RX_WIDTH_MIN 900
#define RX_WIDTH_LOW_FULL 1000
#define RX_WIDTH_LOW_NORM 1100
#define RX_WIDTH_LOW_TRACK 1250
#define RX_WIDTH_MID 1500
#define RX_WIDTH_MODE_MID  1500 //it was on 1550	, changed by mstrens to 1500// Move all hysteresis to Hold Mode side so 1500-1520 will always force Rate Mode
#define RX_WIDTH_HIGH_TRACK 1750
#define RX_WIDTH_HIGH_NORM 1900
#define RX_WIDTH_HIGH_FULL 2000
#define RX_WIDTH_MAX 2100



int16_t stickAilUs_offset, stickElvUs_offset, stickRudUs_offset; 
const int16_t stick_gain_max = 400; // [1100-1500] or [1900-1500] => [0-STICK_GAIN_MAX]
const int16_t master_gain_max = 400; // [1500-1100] or [1500-1900] => [0-MASTER_GAIN_MAX]

int16_t correction[3] = {0, 0, 0};
int16_t correctionPosSide[3] = {0, 0, 0};
int16_t correctionNegSide[3] = {0, 0, 0};

uint32_t lastStabModeUs = 0;
int8_t stabModeCount;



int16_t min(const int16_t a, const int16_t b)
{
    return (b < a) ? b : a;
}


void calculateCorrectionsToApply(){ 
                                // it is called from applyGyroCorrections only if gyroIsInstalled and calibrated before applying PWM on the outputs
                              // it calculates the corrections and split them in positive and negatieve part
                              // at this step, we do not take care of the mixers/ratio/.. defined on the handset and being part of mixer calibration 
    uint32_t t = microsRp(); 
    if ((int32_t)(t - last_pid_time) < PID_PERIOD) return;

    int16_t stickAilUs, stickElvUs, stickRudUs, controlUs ;
    stickAilUs = rcChannelsUs[config.gyroChan[0]-1]; // get original position of the 3 sticks
    stickElvUs = rcChannelsUs[config.gyroChan[1]-1];
    stickRudUs = rcChannelsUs[config.gyroChan[2]-1];
    controlUs = rcChannelsUs[config.gyroChanControl]-1; // get original position of the control channel
    /*  // to debug
    static uint32_t prevPrintMs = 0;
    if ((millisRp() - prevPrintMs) > 1000){
        printf("gyroswitch=%i\n", (int(controlUs)));
        prevPrintMs = millisRp();
    }
    */
    int16_t stick_gain[3];
    int16_t master_gain;
    uint8_t i;

    // stabilization mode
    //STAB_RATE when 988us = switch haut
    //STAB_HOLD when 2012us = switch bas
    enum STAB_MODE stabMode2 = 
      (stabMode == STAB_HOLD && controlUs <= RX_WIDTH_MODE_MID - RX_MODE_HYSTERESIS) ? STAB_RATE : 
      (stabMode == STAB_RATE && controlUs >= RX_WIDTH_MODE_MID + RX_MODE_HYSTERESIS) ? STAB_HOLD : stabMode; // hysteresis, all now in Hold Mode region

    if (stabMode2 != stabMode) {
        stabMode = stabMode2;
        //printf("stabMode changed in updateGyroCorrections() to %i\n", stabMode);
        
        // reset attitude error and i_limit threshold on mode change
        for (i=0; i<3; i++) {
            pid_state.sum_err[i] = 0;
            pid_state.i_limit[i] = 0;
        }

        // check for inflight rx calibration; when swith mode change 3X (ex RATE/HOLD/RATE) with no more that 0.5 sec between each change
        
        //if (cfg.inflight_calibrate == INFLIGHT_CALIBRATE_ENABLE) {
        if ((int32_t)(t - lastStabModeUs) > 500000L) {  // so 2X per sec
        stabModeCount = 0;
        }
        lastStabModeUs = t;

        if (++stabModeCount >= 3) {
            if ( (stickAilUs > 1400) and (stickAilUs < 1600) and (stickElvUs > 1400) and (stickElvUs < 1600) and (stickRudUs > 1400) and (stickRudUs < 1600) ){ 
                gyroMixer.neutralUs[0] = stickAilUs;
                gyroMixer.neutralUs[1] = stickElvUs;
                gyroMixer.neutralUs[2] = stickRudUs;
            }    
        }
        
    }
    
    // determine how much sticks are off center (from neutral)
    stickAilUs_offset = stickAilUs - 1500; // here we suppose that Tx send 1500 us at mid point; otherwise we could use neutral from learning process
    stickElvUs_offset = stickElvUs - 1500;
    stickRudUs_offset = stickRudUs - 1500;
    
    // vr_gain[] [-128, 128] from VRs or config ; in mstrens code, it is supposed to come only from config 
    
    // see enum STICK_GAIN_THROW. shift=0 => FULL, shift=1 => HALF, shift=2 => QUARTER
    //int8_t shift = cfg.stick_gain_throw - 1;
    int8_t shift = config.stick_gain_throw - 1;
    
    // stick_gain[] [1100, <ail*|ele|rud>_in2_mid, 1900] => [0%, 100%, 0%] = [0, STICK_GAIN_MAX, 0]
    stick_gain[0] = stick_gain_max - min(abs(stickAilUs_offset) << shift, stick_gain_max);
    stick_gain[1] = stick_gain_max - min(abs(stickElvUs_offset) << shift, stick_gain_max);
    stick_gain[2] = stick_gain_max - min(abs(stickRudUs_offset) << shift, stick_gain_max);    
    
    // master gain [Rate = 1475-1075] or [Hold = 1575-1975] => [0, MASTER_GAIN_MAX] 
    if (controlUs < (RX_WIDTH_MID - RX_GAIN_HYSTERESIS))  {
    	// Handle Rate Mode Gain Offset, gain = 1 when hysteresis area is exited
    	// Previously gain was the value of RX_GAIN_HYSTERESIS at first exit
    	master_gain = constrain(((RX_WIDTH_MID - RX_GAIN_HYSTERESIS) - controlUs) , 0, master_gain_max);
    } else {
    	if (controlUs > (RX_WIDTH_MODE_MID + RX_MODE_HYSTERESIS))  {
    	   // Handle Hold Mode Gain Offset, gain = 1 when both hysteresis areas are exited
    	   // Previously gain was the value of RX_MODE_HYSTERESIS at first exit
    	   master_gain = constrain(controlUs - (RX_WIDTH_MID + RX_MODE_HYSTERESIS), 0, master_gain_max);		
    	} 
    	  // Force Gain to 0 while in either of the Hysteresis areas    
    	  else  master_gain = 0; // Force deadband
    }	  	
  
    // commanded angular rate (could be from [ail|ele|rud]_in2, note direction/sign)
    if (stabMode == STAB_HOLD || 
        (stabMode == STAB_RATE && config.rate_mode_stick_rotate == RATE_MODE_STICK_ROTATE_ENABLE)) {
        // stick controlled roll rate
        // cfg.max_rotate shift = [1, 5]
        // eg. max stick == 400, cfg.max_rotate == 4. then 400 << 4 = 6400 => 6400/32768*2000 = 391deg/s (32768 == 2000deg/s)
        int16_t sp[3];
        sp[0] = stickAilUs_offset << config.max_rotate;
        sp[1] = stickElvUs_offset << config.max_rotate;
        sp[2] = stickRudUs_offset << config.max_rotate;
        for (i=0; i<3; i++)
            //pid_state.setpoint[i] = vr_gain[i] < 0 ? sp[i] : -sp[i];
            pid_state.setpoint[i] = config.vr_gain[i] < 0 ? sp[i] : -sp[i];      
    } else {
        // zero roll rate, stabilization only
        for (i=0; i<3; i++) 
            pid_state.setpoint[i] = 0;
    }
        
    if (stabMode == STAB_HOLD) {
        // max attitude error (bounding box)    
        for (i=0; i<3; i++) {
            // 2000 deg/s == 32768 units, so 1 deg/(PID_PERIOD=10ms) == 32768/20 units
            pid_state.i_limit[i] = ((int32_t)30 * (32768 / 2 / (PID_PERIOD / 1000)) * stick_gain[i]) >> 9; 
        }
    }
    
    // measured angular rate (from the gyro and apply calibration offset)
    pid_state.input[0] = constrain(gyroY, -8192, 8191);  
    pid_state.input[1] = constrain(gyroX, -8192, 8191);
    pid_state.input[2] = constrain(gyroZ, -8192, 8191);        
    
    // apply PID control
    compute_pid(&pid_state, (stabMode == STAB_RATE) ? &config.pid_param_rate : &config.pid_param_hold);
        
    // apply vr_gain, stick_gain and master_gain
    for (i=0; i<3; i++) {
        // vr_gain [-128,0,127]/128, stick_gain [0,400,0]/512, master_gain [400,0,400]/512
        //correction[i] = ((((int32_t)pid_state.output[i] * vr_gain[i] >> 7) * stick_gain[i]) >> 9) * master_gain >> 9;
        correction[i] = ((((int32_t)pid_state.output[i] * config.vr_gain[i] >> 7) * stick_gain[i]) >> 9) * master_gain >> 9;

        uint16_t OSP = rcChannelsUs[config.gyroChan[i]-1];  // orginal stick position
        uint16_t ESP = OSP + correction[i];                // expected stick position
        if (OSP >= 1500 ){
            if (ESP >= 1500){
            correctionPosSide[i] = correction[i];
            } else {
                correctionPosSide[i] = (1500 - OSP) ; // corr is neg, so Positive part must be negative     
            }
        } else {
            if (ESP >= 1500){
            correctionPosSide[i] = ESP -1500;  // corr is positive, part 1 and 2 must be pos too   
            } else {
                correctionPosSide[i] = 0 ;   
            }
        }
        correctionNegSide[i] = correction[i] - correctionPosSide[i] ;       
    }
    if (msgEverySec(2)) { printf("Mode=%-5i  gx=%-5i  gy=%-5i  gz=%-5i  ct=%-5i  cp=%-5i  cn=%-5i  "  ,\
                 stabMode , pid_state.input[0] , pid_state.input[1],pid_state.input[2]\
                            ,correction[0] , correctionPosSide[0] , correctionNegSide[0]\
    );}
         //                   ,config.vr_gain[0], stick_gain[0] ,  master_gain
    //                    ,pid_state.output[0], pid_state.output[1] , pid_state.output[2]   
    
    /*
    // to do : I need to understand what is calibration_wag
    // this is normally not required with the learning process of oXs (based on 2 switch changes )  
    // calibration wag on all surfaces if needed
    if (calibration_wag_count > 0) {
        if ((int32_t)(t - last_calibration_wag_time) > 200000L) {
            calibration_wag_count--;
            last_calibration_wag_time = t;
        }
        for (i=0; i<3; i++)
            correction[i] += (calibration_wag_count & 1) ? 150 : -150;    
    }    
    */
#if defined(SERIAL_DEBUG) && 0
    Serial.print(correction[0]); Serial.print('\t');
    Serial.print(correction[1]); Serial.print('\t');
    Serial.print(correction[2]); Serial.println('\t');
#endif
    last_pid_time = t;
    // end of gyroCorrectionsToApply
}

void applyGyroCorrections(){
    //This should be called only when new Rc values have been received (called by updatePwm)
    // This function is called only when gyro is used (checked before calling this function) and calibrated
    // corrections are calculated and added in rcChannelsUsCorr[]

    // register min and max Values in order to use them as servo limits.    
    for (uint8_t i=0; i<16;i++){
        if ( rcChannelsUs[i] > gyroMixer.maxUs[i]) {   // automatically update min and max (to adapt the limits from power on - values are not saved in flash)
            gyroMixer.maxUs[i] = rcChannelsUs[i] ;
        } else if (rcChannelsUs[i] < gyroMixer.minUs[i]) {
            gyroMixer.minUs[i] = rcChannelsUs[i] ;
        }    
    }
    calculateCorrectionsToApply(); // recalculate gyro corrections at regular interval but without taking care of the mixers;
    // from here, we know the corrections to apply on the pos and neg sides
    // apply corrections
    for (uint8_t i=0 ; i<16 ; i++){  // for each Rc channel
        if ( gyroMixer.used[i] ) { // when this channel uses gyro correction.
            // adapt the corrections Pos and Neg for each axis based on the ranges stored in gyroMixer.
            // to do : check that those are the correct matching between Pos/neg and Left/right
            if ((msgEverySec(3)) and (i == 1)) {
                printf(" cp=%-5i * rr=%-5i  cn=%-5i * rl=%-5i  ", correctionPosSide[0] , gyroMixer.rateRollRightUs[i],\
                   correctionNegSide[0] , gyroMixer.rateRollLeftUs[i]);
            }
            //imagine that after learning process : RollRigth is negative (mean PWM goes to min), so then RollLeft is positive (always the opposite)
            // imagine that a correction to the right is required and that gives a positive corr applied from neutral.
            //      so  Pos part of corr is positieve and neg part = 0
            // when applied on the servo pwm must changed by pos part (+)* rollright (-) => result is negative (and roll goes to right => OK)
            //
            // imagine a correction to the left is required and so a negative corr is applied from neutral.
            // Pos part is = 0 and neg part is negatieve
            // when applied on the servo pwm must changed by pos part (+)* - rollleft (-) => result is positieve (and roll goes to the Left => OK)
            //
            // imagine that a correction to the right is required and that gives a positive corr but applied from stick at already at X% to the left.
            //      so before correction pwm is e.g. at 1600us.
            //      so  Pos part of corr is positieve and neg part = 0
            // when applied on the servo pwm must changed by pos part (+)* rollright (-) => result is negative (and roll goes to right => OK)
            //
            // imagine a correction to the left is required and that gives a negatieve corr but applied from stick at already at X% to the left.
            //     so before correction pwm is e.g. at 1600us.
            //     so Pos part is = negatieve and neg part is negatieve
            // when applied on the servo pwm must changed 
            //             by pos part (-) *  rollright (-) => result is positieve (and roll goes to the Left => OK)
            //             by neg part (-) * - rollleft (+) => result is positieve (and roll goes to the Left => OK)
            // so as rateRollRightUs and rateRollLeftUs have opposite signs, whe have to reverse the sign when applying the correction.
            rcChannelsUsCorr[i] += ((int32_t) correctionPosSide[0] * (int32_t) gyroMixer.rateRollRightUs[i])  >> 9 ; // division by 512 because full range is about 500.
            rcChannelsUsCorr[i] -= ((int32_t) correctionNegSide[0] * (int32_t) gyroMixer.rateRollLeftUs[i])  >> 9 ; // division by 512 because full range is about 500.
            rcChannelsUsCorr[i] += ((int32_t) correctionPosSide[1] * (int32_t) gyroMixer.ratePitchUpUs[i])  >> 9 ; // division by 512 because full range is about 500.
            rcChannelsUsCorr[i] -= ((int32_t) correctionNegSide[1] * (int32_t) gyroMixer.ratePitchDownUs[i])  >> 9 ; // division by 512 because full range is about 500.
            rcChannelsUsCorr[i] += ((int32_t) correctionPosSide[2] * (int32_t) gyroMixer.rateYawRightUs[i])  >> 9 ; // division by 512 because full range is about 500.
            rcChannelsUsCorr[i] -= ((int32_t) correctionNegSide[2] * (int32_t) gyroMixer.rateYawLeftUs[i])  >> 9 ; // division by 512 because full range is about 500.
            // stay within the limits 
            if (rcChannelsUsCorr[i] < gyroMixer.minUs[i]) rcChannelsUsCorr[i] = gyroMixer.minUs[i] ;
            if (rcChannelsUsCorr[i] > gyroMixer.maxUs[i]) rcChannelsUsCorr[i] = gyroMixer.maxUs[i] ;    
        }
    }
    // here all PWM have been recalculated and can be used for PWM output
}

//enum LEARNING_STATE {LEARNING_OFF, LEARNING_INIT,LEARNING_WAIT, LEARNING_MIXERS, LEARNING_LIMITS, LEARNING_ERROR, LEARNING_OK};

enum LEARNING_STATE learningState = LEARNING_OFF;

    #define MARGIN 50
    #define CENTER_LOW 1500-MARGIN
    #define CENTER_HIGH 1500+MARGIN
    #define END_LOW 1000  // Normal 988
    #define END_HIGH 2000  // normal 2012
    

bool checkForLearning(){ // return true when learning process can start
    // We consider that there are 5 changes within 5sec when the sum of 5 intervals between 2 changes is less than 5 sec.
    // we store the last 5 intervals
    // each time an change occurs, we loose the oldiest   
    static enum STAB_MODE prevStabMode = STAB_RATE;
    static uint32_t lastChangeMs = 0 ;
    static uint32_t intervals[5] = {10000 , 10000 ,10000 ,10000 ,10000}; 
    static uint8_t idx = 0;
    static uint32_t sumIntervals = 50000;
    
    
    if (stabMode == prevStabMode)  {
        return false;
    }
    printf("stabMode changed to %i\n", stabMode);
    prevStabMode = stabMode;
    if ((rcChannelsUs[config.gyroChan[0]-1] <= END_LOW) or (rcChannelsUs[config.gyroChan[0]-1] >= END_HIGH) &&\
        (rcChannelsUs[config.gyroChan[1]-1] <= END_LOW) or (rcChannelsUs[config.gyroChan[1]-1] >= END_HIGH) &&\
        (rcChannelsUs[config.gyroChan[2]-1] <= END_LOW) or (rcChannelsUs[config.gyroChan[2]-1] >= END_HIGH)){ 
        uint32_t t = millisRp();
        uint32_t interval = t - lastChangeMs;
        lastChangeMs = t;
        sumIntervals -= intervals[idx];
        sumIntervals += interval;
        intervals[idx] = interval;
        idx++;
        if (idx >= 5) idx =0; 
        if (sumIntervals < 5000) {
            return true; 
        }
    }
    return false; 
}

void calibrateGyroMixers(){
    static bool centerFlag;
    static uint16_t centerCh[16];  // rc channels when 3 sticks are centered
    //static bool dirFlag ;
    static uint16_t dirCh[3];  // stick positions when all 3 are in the corner = AIL and RUD Rigth and ELV UP
    static bool rightUpFlags[3] ;   // register if we already know the 16 Rc channels for only one of the 3 sticks in low corner 
    static uint16_t rightUpUs[3][16];  // register the 16 Rc channels values 
    static bool leftDownFlags[3] ;    // idem for 3 sticks in opposite corner
    static uint16_t leftDownUs[3][16];
    static enum STAB_MODE prevStabMode ;  // used to detect when gyro switch change
    static uint32_t startMs ;             // used to check that we stay in step 1 for at least 5 sec  
    static uint16_t minLimitsUs[16]; // min limits of the 16 servos during the learning process
    static uint16_t maxLimitsUs[16]; // idem for max limits
    static uint16_t prevStickPosUs[3]; // use to control that the stick are in a stable position.

    if (learningState == LEARNING_OFF){
        if (checkForLearning() == false){  // function return true when conditions to start the learning process are OK  
            return;
        }
        // learning process may start
        // save the current position of sticks
        //dirFlag = true;
        //dirFlag = false;
        dirCh[0] = rcChannelsUs[config.gyroChan[0]-1]; // save the stick values for Ail Right  
        dirCh[1] = rcChannelsUs[config.gyroChan[1]-1]; // save the values for ELV Up
        dirCh[2] = rcChannelsUs[config.gyroChan[2]-1]; // save the values for Rud Right  
        centerFlag = rightUpFlags[0] = rightUpFlags[1] = rightUpFlags[2] = leftDownFlags[0] = leftDownFlags[1] = leftDownFlags[2] = false;
        for (uint8_t i=0; i<16; i++) {  // reset the min and max RC channel limits
            minLimitsUs[i] = 2012; // lower Rc channel values will be discoverd during the learning process
            maxLimitsUs[i] = 988;  // idem for upper values
        }
        //centerCh[16]; dirCh[16]; rightUpUs[3][16]; leftDownUs[3][16]; will be filled with the flags
        startMs = millisRp();         // used to check that there is 5 msec before switching to second step
        ledState = STATE_GYRO_CAL_MIXER_NOT_DONE;
        printf("\nThe learning process started\n");
        learningState = LEARNING_MIXERS;   
    }
    // once learning process has started, we save always the min and max servo positions
    for (uint8_t i=0; i<16;i++){
        if ( rcChannelsUs[i] > maxLimitsUs[i]) {
            maxLimitsUs[i] = rcChannelsUs[i];
        } else if (rcChannelsUs[i] < minLimitsUs[i]) {
            minLimitsUs[i] = rcChannelsUs[i] ; 
        }    
    }
    // when process starts, change led color to red (state= LEARNING_MIXERS)  
    // if state = LEARNING_MIXERS
    // if all 3 sticks are centered (+/-2), set flag and save 16 channels
    // if one axis is > 98% and the 2 others = 0 (+/-2) and if axis is more than previous, save a flag, save the 16 channels
    // if one axis is < 98% and the 2 others = 0 (+/-2) and if axis is less than previous, save a flag, save the 16 channels
    // when all 7 cases have been discoverd, state => LEARNING_MIXERS_DONE and change LED color to blue. 
    // if stabMode change (other than OFF) after 5 sec
    //    if state is not LEARNING _MIXERS_DONE, this is an error => set config in error (this will block everything)
    //    if ok (all flags set), set state to LEARNING_LIMITS
    // Wait for a new change before saving the parameters and change state to LEARNING_OFF
    // LED color is managed is main loop based on the state. 

    // this part just help to have clearer and shorter code here after
    int16_t stickPosUs[3];  // 3 original stick positions
    bool inCenter[3] = {false, false,false};     // true when stick is centered (with some tolerance)
    bool inCorner[3] = {false, false,false};        // true when stick is in a corner (with some tolerance)
    uint8_t i;
    bool sticksMaintained = true;
    stickPosUs[0] = (int16_t) rcChannelsUs[config.gyroChan[0]-1]; 
    stickPosUs[1] = (int16_t) rcChannelsUs[config.gyroChan[1]-1]; 
    stickPosUs[2] = (int16_t) rcChannelsUs[config.gyroChan[2]-1];
    for (i=0;i<3;i++){                // detect when sticks are centered or in a corner
        if ((stickPosUs[i] >= CENTER_LOW) and (stickPosUs[i] <= CENTER_HIGH)) inCenter[i] = true;
        if ((stickPosUs[i] <= END_LOW) or (stickPosUs[i] >= END_HIGH)) inCorner[i] = true;
        if (stickPosUs[i] != prevStickPosUs[i]) sticksMaintained = false;
        prevStickPosUs[i] = stickPosUs[i];
    }
    // then process depend on the state 
    if ((learningState == LEARNING_MIXERS) or (learningState == LEARNING_MIXERS_DONE)){
        // if all 3 sticks are centered, set flag and save 16 channels in centerCh
        if ((inCenter[0] && inCenter[1] && inCenter[2] ) and sticksMaintained){
            if (centerFlag == false){
                printf("centered\n");
            }
            centerFlag = true;
            memcpy(centerCh,rcChannelsUs, sizeof(rcChannelsUs) );
            
        } else if (inCorner[0] && inCenter[1] && inCenter[2] && sticksMaintained){ // Ail at end
            i=0;  // Aileron
            if ((abs(stickPosUs[i] - dirCh[i]) < 100) ) { //stick is in the right or up corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < rightUpUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > rightUpUs[i][config.gyroChan[i]-1]) or\
                    (rightUpFlags[i] == false)){\
                    memcpy(rightUpUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (rightUpFlags[i] == false) {
                    printf("Ail right Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", rightUpUs[i][0] , rightUpUs[i][1] , rightUpUs[i][2] ,rightUpUs[i][3]); // to remove
                }
                rightUpFlags[i] = true;    // save when new pos is lower than previous
            } else {  // stick is in the left or down corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < leftDownUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > leftDownUs[i][config.gyroChan[i]-1]) or\
                    (leftDownFlags[i] == false)){\
                    memcpy(leftDownUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (leftDownFlags[i] == false) {
                    printf("Ail left Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", leftDownUs[i][0] , leftDownUs[i][1] , leftDownUs[i][2] , leftDownUs[i][3]); // to remove
                }
                leftDownFlags[i] = true;    // save when new pos is lower than previous
            }    
        } else if (inCorner[1] && inCenter[0] && inCenter[2] && sticksMaintained){ // ELV at end
            i=1;  // Elv
            if ((abs(stickPosUs[i] - dirCh[i]) < 100)) { //stick is in the right or up corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < rightUpUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > rightUpUs[i][config.gyroChan[i]-1]) or\
                    (rightUpFlags[i] == false)){\
                    memcpy(rightUpUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (rightUpFlags[i] == false) {
                    printf("Elv up Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", rightUpUs[i][0] , rightUpUs[i][1] , rightUpUs[i][2] ,rightUpUs[i][3]); // to remove
                }
                rightUpFlags[i] = true;    // save when new pos is lower than previous
            } else {  // stick is in the left or down corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < leftDownUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > leftDownUs[i][config.gyroChan[i]-1]) or\
                    (leftDownFlags[i] == false)){\
                    memcpy(leftDownUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (leftDownFlags[i] == false) {
                    printf("Elv down Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", leftDownUs[i][0] , leftDownUs[i][1] , leftDownUs[i][2] , leftDownUs[i][3]); // to remove
                }
                leftDownFlags[i] = true;    // save when new pos is lower than previous
            }    
        } else if (inCorner[2] && inCenter[0] && inCenter[1] && sticksMaintained) { // Rud at end
            i=2; // 
            if ((abs(stickPosUs[i] - dirCh[i]) < 100)) { //stick is in the right or up corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < rightUpUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > rightUpUs[i][config.gyroChan[i]-1]) or\
                    (rightUpFlags[i] == false)){\
                    memcpy(rightUpUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (rightUpFlags[i] == false) {
                    printf("Rud right Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", rightUpUs[i][0] , rightUpUs[i][1] , rightUpUs[i][2] ,rightUpUs[i][3]); // to remove
                }
                rightUpFlags[i] = true;    // save when new pos is lower than previous
            } else {  // stick is in the left or down corner
                if (( stickPosUs[i] < 1500) and ( stickPosUs[i] < leftDownUs[i][config.gyroChan[i]-1]) or\
                    ( stickPosUs[i] > 1500) and ( stickPosUs[i] > leftDownUs[i][config.gyroChan[i]-1]) or\
                    (leftDownFlags[i] == false)){\
                    memcpy(leftDownUs[i],rcChannelsUs, sizeof(rcChannelsUs) );
                }
                if (leftDownFlags[i] == false) {
                    printf("Rud left Ch1=%i   Ch2=%i    Ch3=%i    Ch4=%i\n", leftDownUs[i][0] , leftDownUs[i][1] , leftDownUs[i][2] , leftDownUs[i][3]); // to remove
                }
                leftDownFlags[i] = true;    // save when new pos is lower than previous
            }    
        }
        // detect when all cases have been performed at least once (all flags are true)
        if ((centerFlag==true) && (rightUpFlags[0]==true) && (leftDownFlags[0]==true)\
             && (rightUpFlags[1]==true) && (leftDownFlags[1]==true) && (rightUpFlags[2]==true) && (leftDownFlags[2]==true)){
            ledState = STATE_GYRO_CAL_MIXER_DONE;
            learningState = LEARNING_MIXERS_DONE; // this will change the led color 
            static uint32_t prevPrintMs = 0;
            if ((millisRp() - prevPrintMs) > 1000){
                //printf("Ail=%i  Elv=%i    Rud=%i\n", stickPosUs[0] , stickPosUs[1] , stickPosUs[2] );
                printf("Cal mixer is done; switch may be changed\n");
                prevPrintMs=millisRp();
            }
    
        } 
        // check for a switch change
        // discard the changes done in the first 5 sec
        // if change occurs after 5 secin case of switch change when   check for error        
        if ( stabMode != prevStabMode)  { 
            prevStabMode = stabMode;
            if (( millisRp() - startMs) > 5000) { // when change, occurs after 5 sec of starting the learning process
                if (learningState == LEARNING_MIXERS) {  // error
                    if ( centerFlag == false) {printf("Error in Gyro setup: missing all 3 sticks centered\n");}
                    if ( ( rightUpFlags[0] == false) or ( leftDownFlags[0] == false) ) {printf("Error in Gyro setup: missing one corner position for aileron alone\n");}  
                    if ( ( rightUpFlags[1] == false) or ( leftDownFlags[1] == false) ) {printf("Error in Gyro setup: missing one corner position for elevator alone\n");}  
                    if ( ( rightUpFlags[2] == false) or ( leftDownFlags[2] == false) ) {printf("Error in Gyro setup: missing one corner position for rudder alone\n");}  
                    printf("Error :  oXs will stop handling receiver and sensors; power off/on is required !!!!!!");
                    watchdog_update();
                    ledState = STATE_NO_SIGNAL;
                    learningState = LEARNING_OFF;    
                    // to do : stop all interrupts to avoid error messages because queues are full
                    // disable interrupt to avoid having to error msg about queue being full 
                    uart_set_irq_enables(CRSF2_UART_ID, false, false);
                    uart_set_irq_enables(CRSF2_UART_ID, false, false);
                    uart_set_irq_enables(SBUS_UART_ID, false, false);
                    uart_set_irq_enables(SBUS2_UART_ID, false, false);
    
    
                    configIsValid = false;
                } else {
                    ledState = STATE_GYRO_CAL_LIMIT;
                    printf("\nSecond step of learning process started\n");
                    learningState = LEARNING_LIMITS;
                }    
            }    
        }        
    } else if (learningState == LEARNING_LIMITS){
        // wait for a new change of switch to save and close
        if ( stabMode != prevStabMode) { // when change, 
            //build the parameters and save them
            struct gyroMixer_t temp; // use a temporary structure
            temp.version = GYROMIXER_VERSION;
            temp.isCalibrated = true; 
            for (i=0;i<16;i++) {
                #define MM 50    // MM = MIXER MARGIN
                // look at the channels that have to be controled by the gyro.
                // channel is used if there are differences and channel is not a stick
                temp.used[i] = false;
                uint8_t j=i++;
                if ( ((j != config.gyroChan[0] ) and (j != config.gyroChan[1] ) and (j != config.gyroChan[2])  and ( j!= config.gyroChanControl ))\
                 and ((abs(rightUpUs[0][i]-leftDownUs[0][i])>MM) or (abs(rightUpUs[0][i]-centerCh[i])>MM) or (abs(leftDownUs[0][i]-centerCh[i])>MM)\
                    or(abs(rightUpUs[1][i]-leftDownUs[1][i])>MM) or (abs(rightUpUs[1][i]-centerCh[i])>MM) or (abs(leftDownUs[1][i]-centerCh[i])>MM)\
                    or(abs(rightUpUs[2][i]-leftDownUs[2][i])>MM) or (abs(rightUpUs[2][i]-centerCh[i])>MM) or (abs(leftDownUs[2][i]-centerCh[i])>MM))) {
                    temp.used[i] = true; 
                }
                temp.neutralUs[i] = centerCh[i];  // servo pos when 3 stricks are centered
                
                temp.rateRollRightUs[i] =  (int16_t) rightUpUs[0][i] - (int16_t) centerCh[i] ;//   then right rate
                temp.rateRollLeftUs[i] =  (int16_t) leftDownUs[0][i] - (int16_t) centerCh[i] ;//   then LEFT rate 
                temp.ratePitchUpUs[i] =    (int16_t) rightUpUs[1][i] - (int16_t) centerCh[i] ;//   then right rate
                temp.ratePitchDownUs[i] = (int16_t) leftDownUs[1][i] - (int16_t) centerCh[i] ;//   then LEFT rate 
                temp.rateYawRightUs[i] =   (int16_t) rightUpUs[2][i] - (int16_t) centerCh[i] ;//   then right rate
                temp.rateYawLeftUs[i]  =  (int16_t) leftDownUs[2][i] - (int16_t) centerCh[i] ;//   then LEFT rate 
                
                temp.minUs[i] = minLimitsUs[i];
                temp.maxUs[i] = maxLimitsUs[i]; 
            }  // end for (all axis processed)
            ledState = STATE_NO_SIGNAL;
            learningState = LEARNING_OFF;
            printf("\nGyro calibration:\n");
            printf("Stick Ail_Right on channel %i at %-5i\n", config.gyroChan[0], pc(dirCh[0] - 1500));
            printf("Stick Elv Up    on channel %i at %-5i\n", config.gyroChan[1], pc(dirCh[1] - 1500));
            printf("Stick Rud_Right on channel %i at %-5i\n", config.gyroChan[2], pc(dirCh[2] - 1500));
            printf("Gyro corrections (from center pos in %%) on:      \n");
            for (uint8_t i = 0; i<16;i++) {
                if ( temp.used[i]) {
                    printf("Channel %-2i center=%-4i rollRight=%-4i rollLeft=%-4i pitchUp%-4i pitchDown=%-4i yawRight=%-4i yawLeft=%-4i min=%-4i max=%-4i\n",\
                    i+1 , pc(temp.neutralUs[i]-1500) , pc(temp.rateRollRightUs[i]), pc(temp.rateRollLeftUs[i]), \
                    pc(temp.ratePitchUpUs[i]) , pc(temp.ratePitchDownUs[i]),\
                    pc(temp.rateYawRightUs[i]), pc(temp.rateYawLeftUs[i]),\
                    pc(temp.minUs[i]-1500) , pc(temp.maxUs[i]-1500) ) ;
                }
            }
            // update gyroMixer from temp.
            memcpy(&gyroMixer , &temp, sizeof(temp));
            printf("\nEnd of learning process; result will be saved in flash\n");
            saveGyroMixer(); //   save the gyro mixer from the learning process.                     
        } // end switch change
    }    
} 

int pc(int16_t val){ // convert a Rc value (-512/512) in %
    return ((int) val * 100 )>>9;  //divide by 512 = 100% 
}