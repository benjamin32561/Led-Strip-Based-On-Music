#include "BluetoothA2DPSink.h"
#include "FFT.h"

#define MAX_VAL 32767 //max positive value of 16 bit amplitude
#define MAX_AMP 65535 //value range of 16 bit amp
#define SAMPLING_FREQ 44100 //this is the spotify sample rate
//hyperparameters
#define FFT_N 2048 //must be power of 2, number of samples for fft calc
#define BASS_START_FREQ 20 //the starting frequncy to search spikes at
#define BASS_END_FREQ 150 //the ending frequncy to search spikes at
#define MAX_RGB_SPECTRUM 767 //RGB spectrum is 0-767
#define MIN_MUL 0.01 //minimum amplitude to calculate fft
#define MAX_MAG 650 //highet frequency magnetiude, this is not the real highest value, it just work well
//the number of the LED pin
#define R_PIN 14
#define G_PIN 13
#define B_PIN 32
//channels
#define R_CH 0
#define G_CH 1
#define B_CH 2
//PWM CONST
#define FREQ 5000
#define RES 8

typedef struct rgb{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb;

BluetoothA2DPSink a2dp_sink;

rgb colors_state; //to save current led state
float total_time = FFT_N*1.0/SAMPLING_FREQ; //total time for sample
float fft_input[FFT_N]; //fft input array
float fft_output[FFT_N]; //fft output array
uint16_t input_index = 0;
uint16_t color_index = 0;
int max_amp_in_buff = 0;
fft_config_t* real_fft_plan = fft_init(FFT_N, FFT_REAL, FFT_FORWARD, fft_input, fft_output);

/*
this function is used to set up a pin at a certain channel for each led
*/
void setLed(int ch, int pin){
  ledcSetup(ch, FREQ, RES);
  ledcAttachPin(pin, ch);
}

/*
this function is used for changing the value of certain channel at a smooth way
*/
void changeSpecificLed(int ch, int new_val, int old_val){
  if (new_val>old_val){
    for (int i = old_val; i<=new_val; i++)
      ledcWrite(ch,i);
  }
  else{
    for (int i = old_val; i>=new_val; i--)
      ledcWrite(ch,i);
  }
}

/*
this function is used for changing the leds to new values
*/
void changeTo(uint8_t r, uint8_t g, uint8_t b){
  //changing color of leds
  changeSpecificLed(R_CH,r,colors_state.r);
  changeSpecificLed(G_CH,g,colors_state.g);
  changeSpecificLed(B_CH,b,colors_state.b);
  //updating current state of leds
  colors_state.r = r;
  colors_state.g = g;
  colors_state.b = b;
}

/*
this function is used for changing the led color
*/
void changeByMusic(float mul){
  rgb rgb_data;
  rgb_data.r = 0;
  rgb_data.g = 0;
  rgb_data.b = 0;
  uint16_t mapped_rgb = color_index;//color_index is a value between 0 and 767, 
  //it represents the place on the color spectrum
  if (mapped_rgb<=255){
    rgb_data.r = 255-mapped_rgb;
    rgb_data.g = mapped_rgb;
  }else if(mapped_rgb<511){
    mapped_rgb-=255;
    rgb_data.g = 255-mapped_rgb;
    rgb_data.b = mapped_rgb;
  }else{
    mapped_rgb-=511;
    rgb_data.b = 255-mapped_rgb;
    rgb_data.r = mapped_rgb;
  }
  //progressing on the color spectrum for color changes
  color_index++;
  color_index%=MAX_RGB_SPECTRUM;
  //multiplying the values by the multiplier
  rgb_data.r *= mul;
  rgb_data.g *= mul;
  rgb_data.b *= mul;
  //updating the colors
  changeTo(rgb_data.r,rgb_data.g,rgb_data.b);
}

/*
this function is the reader callback for the A2DP connection
*/
void read_data(const uint8_t *data, uint32_t len){
   uint32_t n = len/2;
   int16_t* sound_pointer=(int16_t*)data;

   for(uint32_t i=0;i<n;i++){
    int16_t sound = *sound_pointer;
    real_fft_plan->input[input_index] = sound*1.0/MAX_VAL;
    
    if (max_amp_in_buff<abs(sound))max_amp_in_buff=abs(sound);//saving highest amplitude in the fft input
    input_index++;
    //if the fft input array is full
    if (input_index == FFT_N)
    {
      //calculating multiplier
      float mul = max_amp_in_buff*1.0/MAX_VAL;
      if (mul >= MIN_MUL)//the multiplier is highest than the min threshold
      {    
        //calculating fft
        fft_execute(real_fft_plan);
        float max_magnitude = 0;
        float fundamental_freq = 0;
        //finding the main frequncy in the fft buffer
        for (int k = 0 ; k < real_fft_plan->size / 2 ; k++)
        {
          float mag = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2));
          //if the frequency is above the max relevnt frequency breaking
          if (k/total_time > BASS_END_FREQ) break;
          if(mag > max_magnitude)
          {
              max_magnitude = mag;
              fundamental_freq = k/total_time;
          }
        }
        //calculating the led multiplier
        mul = max_magnitude/MAX_MAG;
        if(mul>1.0)mul=1.0;
        if (fundamental_freq>=BASS_START_FREQ)//if the frequency is in the range of relevent frequencys
          changeByMusic(mul);
      }
      else changeTo(0,0,0); //if the amplitude is not high enough, turning off the led
      input_index = 0;
      max_amp_in_buff = 0;
    }
    
    sound_pointer++;
  }
}


void setup() {
  setLed(R_CH,R_PIN);
  setLed(G_CH,G_PIN);
  setLed(B_CH,B_PIN);

  colors_state.r = 0;
  colors_state.g = 0;
  colors_state.b = 0;
  
  changeTo(0,0,0);
  
  a2dp_sink.set_stream_reader(read_data);
  a2dp_sink.start("ESP32_SPEAKER");
}

void loop() {
}
