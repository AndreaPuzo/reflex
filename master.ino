#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MINUTES(x)    ((x) * 60000)

enum {
  MY_STATE_HALT         ,
  MY_STATE_RESET        ,
  MY_STATE_TEST_START   ,
  MY_STATE_TEST_STOP    ,
  MY_STATE_TEST_ANALIZE ,
  MY_STATE_TEST_PAUSE   ,
  MY_STATE_TEST_RESUME  ,
  MY_STATE_TEST_LED     ,
  MY_STATE_TEST_BUTTON  ,
  MY_STATE_WRITE_MSB    ,
  MY_STATE_WRITE_LSB    ,
} ;

#define MY_PIN_CLOCK  2
#define MY_PIN_VALUE  3
#define MY_PIN_KEY    4
#define MY_PIN_LOCK   5
#define MY_PIN_LED    6
#define MY_PIN_RESET  7
#define MY_PIN_BUTTON 8

#define MY_TEST_LED_MIN_TIME 300
#define MY_TEST_LED_MAX_TIME 1000
#define MY_TEST_MAX_SIZE     500

volatile unsigned char my_state ;
unsigned short my_value         ;
unsigned long  my_last_time     ;
unsigned long  my_timeout       ;
unsigned long  my_test_LED_time ;
unsigned long  my_test_timeout  ;
unsigned short my_test_size     ;
unsigned short my_test_max_size ;
char           my_input [ 64 + 1 ] ;
unsigned short my_test_data [ MY_TEST_MAX_SIZE ]  ;

#define MY_ELAPSED_MILLIS (millis() - my_last_time)

/*** Main ***/

void setup ()
{  
  // setup the serial connection
    
  Serial.begin(9600) ;

  while (!Serial) {
    
  }
  
  Serial.print("*** Serial monitor connected ***\n") ;

  // setup the PINs

  pinMode(MY_PIN_CLOCK  , OUTPUT) ;
  pinMode(MY_PIN_VALUE  , OUTPUT) ;
  pinMode(MY_PIN_KEY    , OUTPUT) ;
  pinMode(MY_PIN_LOCK   ,  INPUT) ;
  pinMode(MY_PIN_LED    , OUTPUT) ;
  pinMode(MY_PIN_RESET  ,  INPUT) ;
  pinMode(MY_PIN_BUTTON ,  INPUT) ;
  
  // set the Interrupt Service Routine (ISR) when the
  // input of pin `MY_PIN_RESET` is switching from LOW
  // to HIGH (= RISING)
  attachInterrupt(
    digitalPinToInterrupt(MY_PIN_RESET) ,
    _OnReset                            ,
    RISING
  ) ;
  
  // reset the machine
  myReset() ;
}

void loop ()
{
  int change = myInput() ;
  
  switch (my_state) {  
  case MY_STATE_HALT :
    if (change) {
      my_timeout   = MINUTES(5) ;
      my_last_time = millis() ;
    } else if (my_timeout <= MY_ELAPSED_MILLIS) {
      Serial.print("*** Ready to run the test ***\n") ;
      my_last_time = millis() ;
    }
    break ;
  
  case MY_STATE_RESET :
    myReset() ;
    break ;
  
  case MY_STATE_TEST_START :
    myTestStart() ;
    break ;
  
  case MY_STATE_TEST_STOP :
    myTestStop() ;
    break ;
  
  case MY_STATE_TEST_ANALIZE :
    myTestAnalize() ;
    break ;
  
  case MY_STATE_TEST_PAUSE :
    if (change) {
      my_timeout   = MINUTES(5) ;
      my_last_time = millis() ;
    } else if (my_timeout <= MY_ELAPSED_MILLIS) {
      Serial.print("*** Ready to continue the test ***\n") ;
      my_last_time = millis() ;
    }
    break ;

  case MY_STATE_TEST_RESUME :
    myTestRandLED() ;
    break ;
    
  case MY_STATE_TEST_LED :
    myTestUpdateLED() ;
    break ;
    
  case MY_STATE_TEST_BUTTON :
    myTestUpdateButton() ;
    break ;
  
  case MY_STATE_WRITE_MSB :
    if (myRegisterShiftOut(8)) {
      my_state = MY_STATE_WRITE_LSB ;
    }
    break ;
  
  case MY_STATE_WRITE_LSB :
    if (myRegisterShiftOut(0)) {
      my_state = MY_STATE_HALT ;
    }
    break ;
  }
}

/*** C-String functions ***/

#define strequ(dst, src)     (0 == strcmp(dst, src))
#define strnequ(dst, src, n) (0 == strncmp(dst, src, n))

/*** Serial monitor functions ***/

int myInput ()
{
  unsigned char last_state = my_state ;
  
  if (Serial.available()) {
    delay(10) ;
    
    // read the entire line from the port
    
    unsigned int i ;
    bool is_storing = false ;
    
    for (i = 0 ; i < ARRAY_SIZE(my_input) - 1 ;) {
      int ch = Serial.read() ;
      
      if (ch == '\n')
        break ;
      
      if (isAscii(ch)) {
        if (isWhitespace(ch)) {
          if (is_storing) {
            my_input[i++] = (char)ch ;
          }
        } else {
          if (!is_storing) {
            is_storing = true ;
          }
          
          my_input[i++] = (char)ch ;
        }
      }
    }
    
    my_input[i] = 0 ; // set the EOS (End Of String)
    
    // scan the input command
    
    if (strequ(my_input, "halt")) {
      my_state = MY_STATE_HALT ;
    } else if (strequ(my_input, "reset")) {
      my_state = MY_STATE_RESET ;
    } else if (strequ(my_input, "test-start")) {
      my_state = MY_STATE_TEST_START ;
    } else if (strequ(my_input, "test-stop")) {
      my_state = MY_STATE_TEST_STOP ;
    } else if (strequ(my_input, "test-pause")) {
      my_state = MY_STATE_TEST_PAUSE ;
    } else if (strequ(my_input, "test-resume")) {
      my_state = MY_STATE_TEST_RESUME ;
    } else if (strequ(my_input, "test-analize")) {
      my_state = MY_STATE_TEST_ANALIZE ;
    } else {
      Serial.print("*** Error: unknown command ***\n") ;
    }
  }
  
  return (int)my_state - (int)last_state ;
}

/*** Soft(ware) Reset function ***/

void _OnReset ()
{
  my_state = MY_STATE_RESET ;
}

void myReset ()
{
  {    
    my_state = MY_STATE_HALT ;
    
    // setup the PINs
    
    digitalWrite(MY_PIN_CLOCK ,  LOW) ;
    digitalWrite(MY_PIN_VALUE ,  LOW) ;
    digitalWrite(MY_PIN_KEY   ,  LOW) ;
    digitalWrite(MY_PIN_LED   ,  LOW) ;
    
    // lock the register
    myRegisterLock() ;
  }

  Serial.print("*** System has been reset ***\n") ;
  
  // setup the variables
  
  my_timeout   = MINUTES(5) ;
  my_last_time = millis() ;
}

/*** Test functions ***/

void myTestRandLED ()
{
  // randomize the LED time
  my_test_LED_time = (unsigned long)random(
    MY_TEST_LED_MIN_TIME, MY_TEST_LED_MAX_TIME
  ) ;
  
  // check the LED state
  my_state = MY_STATE_TEST_LED ;
  my_last_time = millis() ;
}

void myTestStart ()
{
  // reset the variables
  my_test_size = 0 ;
  my_test_max_size = 20 ;
  my_test_timeout = 1500 ;

  // start the test
  myTestRandLED() ;
}

void myTestStop ()
{
  // turn off the LED
  digitalWrite(MY_PIN_LED, LOW) ;
  
  // analize the sample
  my_state = MY_STATE_TEST_ANALIZE ;
}

void myTestAnalize ()
{
  if (my_test_size <= 2) {
    Serial.print("*** Error: the sample is too poor ***\n") ;
    my_state = MY_STATE_HALT ;
    return ;
  }
  
  unsigned short i, n = my_test_size ;
  unsigned short *  x = my_test_data ;
  
  // compute the mean
  
  float mean = 0.f ;
  
  for (i = 0 ; i < n ; ++i) {
    mean += (float)x[i] ;
  }
  
  mean /= n ;
  
  // compute the standard deviation
  
  float sd = 0.f ;
  
  for (i = 0 ; i < n ; ++i) {
    sd += sq((float)x[i] - mean) ;
  }
  
  sd = sqrt(sd / (n - 2)) ;

  Serial.print(String("Mean    = ") + String(mean) + "\n") ;
  Serial.print(String("Std dev = ") + String(sd) + "\n") ;
  
  // send the results to the PC
  
  Serial.print("$0$") ;
  for (i = 0 ; i < n ; ++i) {
    Serial.print(x[i]) ; Serial.print(";") ;
  }
  Serial.print("\n") ;
  
  // send the mean to the display
  
  my_value = (unsigned short)round(mean) ;
  my_state = MY_STATE_WRITE_MSB ;
}

void myTestUpdateLED ()
{
  if (my_test_LED_time <= MY_ELAPSED_MILLIS) {    
    // turn on the LED
    digitalWrite(MY_PIN_LED, HIGH) ;
        
    // check the button state
    my_state = MY_STATE_TEST_BUTTON ;
    my_last_time = millis() ;
  }
}

void myTestUpdateButton ()
{  
  if (HIGH == digitalRead(MY_PIN_BUTTON)) {
    // compute the elapsed time
    long delta = (long)MY_ELAPSED_MILLIS ;
    
    // turn off the LED
    digitalWrite(MY_PIN_LED, LOW) ;
    
    if (delta < 100 || delta > 9999) {
      Serial.print(
        "*** Warning: the delta time is out of range [100;9999] ***\n"
      ) ;
      
      // discard the measure
      myTestRandLED() ;
    } else {
      // NOTE `delta` can be stored in a 16-bit unsigend variable
      
      // save the current measure
      my_test_data[my_test_size++] = (unsigned short)delta ;
      
      if (my_test_size == my_test_max_size) {
        // stop the test
        my_state = MY_STATE_TEST_STOP ;
      } else {
        // next measure
        myTestRandLED() ;
      }
    }
  } else if (my_test_timeout <= MY_ELAPSED_MILLIS) {
    // turn off the LED
    digitalWrite(MY_PIN_LED, LOW) ;
    
    Serial.print("*** Warning: the time run out ***\n") ;
    
    // discard the measure
    myTestRandLED() ;
  }
}

/*** Register functions ***/

bool myRegisterIsLocked ()
{
  return HIGH == digitalRead(MY_PIN_LOCK) ;
}

void myRegisterLock ()
{
  while (HIGH != digitalRead(MY_PIN_LOCK)) {
    digitalWrite(MY_PIN_KEY , HIGH) ;
    digitalWrite(MY_PIN_KEY ,  LOW) ;
  }
}

void myRegisterUnlock ()
{
  while (HIGH == digitalRead(MY_PIN_LOCK)) {
    digitalWrite(MY_PIN_KEY , HIGH) ;
    digitalWrite(MY_PIN_KEY ,  LOW) ;
  }
}

bool myRegisterShiftOut (int sh)
{
  if (!myRegisterIsLocked())
    return false ;
  
  // write data into the register
  
  unsigned char x = (unsigned char)(my_value >> sh) ;
  
  Serial.print("*** Register contains 0B") ;
  for (int i = 7 ; i >= 0 ; --i) {
    int b = (x >> i) & 1 ;
    Serial.print(b) ;
    digitalWrite(MY_PIN_VALUE , b) ;
    digitalWrite(MY_PIN_CLOCK , HIGH) ;
    digitalWrite(MY_PIN_CLOCK ,  LOW) ;
  }
  Serial.print(" ***\n") ;

  digitalWrite(MY_PIN_VALUE, LOW) ;
  
  // unlock the register
  myRegisterUnlock() ;
  
  return true ;
}
