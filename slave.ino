#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

enum {
  MY_STATE_HALT     ,
  MY_STATE_READ_MSB ,
  MY_STATE_READ_LSB ,
  MY_STATE_RESET    ,
} ;

#define MY_PIN_CLOCK  2
#define MY_PIN_VALUE  3
#define MY_PIN_KEY    4
#define MY_PIN_LOCK   5
#define MY_PIN_RESET  7
#define MY_PIN_8S_A   6
#define MY_PIN_8S_B   8
#define MY_PIN_8S_C   9
#define MY_PIN_8S_D   10
#define MY_PIN_8S_E   11
#define MY_PIN_8S_F   12
#define MY_PIN_8S_G   13
#define MY_PIN_8S_0   A0
#define MY_PIN_8S_1   A1
#define MY_PIN_8S_2   A2
#define MY_PIN_8S_3   A3
#define MY_PIN_8S_DOT A4

volatile unsigned char my_state ;
unsigned short         my_value ;

unsigned char my_8s_num [] = {
  10 , // 1st digit
  10 , // 2nd digit
  10 , // 3rd digit
  10   // 4th digit
} ;

unsigned char my_8s_tab [] = {
  0b11111100 , // 0
  0b01100000 , // 1
  0b11011010 , // 2
  0b11110010 , // 3
  0b01100110 , // 4
  0b10110110 , // 5
  0b10111110 , // 6
  0b11100000 , // 7
  0b11111110 , // 8
  0b11100110 , // 9
  0b00000000   // -
} ;

char my_input [ 64 + 1 ] ;

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
  pinMode(MY_PIN_VALUE  ,  INPUT) ;
  pinMode(MY_PIN_KEY    , OUTPUT) ;
  pinMode(MY_PIN_LOCK   ,  INPUT) ;
  pinMode(MY_PIN_8S_A   , OUTPUT) ;
  pinMode(MY_PIN_8S_B   , OUTPUT) ;
  pinMode(MY_PIN_8S_C   , OUTPUT) ;
  pinMode(MY_PIN_8S_D   , OUTPUT) ;
  pinMode(MY_PIN_8S_E   , OUTPUT) ;
  pinMode(MY_PIN_8S_F   , OUTPUT) ;
  pinMode(MY_PIN_8S_G   , OUTPUT) ;
  pinMode(MY_PIN_8S_DOT , OUTPUT) ;
  pinMode(MY_PIN_8S_0   , OUTPUT) ;
  pinMode(MY_PIN_8S_1   , OUTPUT) ;
  pinMode(MY_PIN_8S_2   , OUTPUT) ;
  pinMode(MY_PIN_8S_3   , OUTPUT) ;

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
  my8SRenderNumber() ;
  myInput() ;
  
  switch (my_state) {  
  case MY_STATE_HALT :
    if (myRegisterIsLocked()) {
      my_state = MY_STATE_READ_MSB ;
    }
    break ;
  
  case MY_STATE_RESET :
    myReset() ;
    break ;
  
  case MY_STATE_READ_MSB :
    if (myRegisterShiftIn(8)) {
      my_state = MY_STATE_READ_LSB ;
    }
    break ;
  
  case MY_STATE_READ_LSB :
    if (myRegisterShiftIn(0)) {
      my8SConvertNumber() ;
      my8SRenderNumber() ;
      my_state = MY_STATE_HALT ;
      Serial.print(String("Mean = ") + String(my_value) + "\n") ;
    }
    break ;
  }
}

/*** C-String functions ***/

#define strequ(dst, src)     (0 == strcmp(dst, src))
#define strnequ(dst, src, n) (0 == strncmp(dst, src, n))

/*** Serial monitor functions ***/

void myInput ()
{  
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
    } else {
      Serial.print("*** Error: unknown command ***\n") ;
    }
  }
}

# define E_8S LOW
# define D_8S HIGH

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
    
    digitalWrite(MY_PIN_CLOCK  , LOW) ;
    digitalWrite(MY_PIN_VALUE  , LOW) ;
    digitalWrite(MY_PIN_KEY    , LOW) ;
    digitalWrite(MY_PIN_8S_DOT , LOW) ; // always LOW

    digitalWrite(MY_PIN_8S_0 , D_8S) ;
    digitalWrite(MY_PIN_8S_1 , D_8S) ;
    digitalWrite(MY_PIN_8S_2 , D_8S) ;
    digitalWrite(MY_PIN_8S_3 , D_8S) ;
    digitalWrite(MY_PIN_8S_A ,  LOW) ;
    digitalWrite(MY_PIN_8S_B ,  LOW) ;
    digitalWrite(MY_PIN_8S_C ,  LOW) ;
    digitalWrite(MY_PIN_8S_D ,  LOW) ;
    digitalWrite(MY_PIN_8S_E ,  LOW) ;
    digitalWrite(MY_PIN_8S_F ,  LOW) ;
    digitalWrite(MY_PIN_8S_G ,  LOW) ;
    
    // wait for the register connection

    while (myRegisterIsLocked()) {
      Serial.println("Looking for arduino...") ;
    }
  }

  Serial.print("*** System has been reset ***\n") ;
  
  my_value = 0 ;
  my8SConvertNumber() ;
}

/*** 8-Segments Display ***/

void my8SConvertNumber ()
{
  unsigned short x = my_value ;
  
  // convert `x` into its 8-segment representation
  
  my_8s_num[0] = x % 10 ; x /= 10 ;
  my_8s_num[1] = x % 10 ; x /= 10 ;
  my_8s_num[2] = x % 10 ; x /= 10 ;
  my_8s_num[3] = x % 10 ; x /= 10 ;
  
  // remove trailing zeros
  
  for (unsigned int i = 3 ; i > 0 && 0 == my_8s_num[i] ; --i) {
    my_8s_num[i] = 10 ;
  }
}

void my8SRenderDigit (unsigned char x)
{
  unsigned char y = my_8s_tab[x] ;
  
  digitalWrite(MY_PIN_8S_A, (y >> 7) & 1) ;
  digitalWrite(MY_PIN_8S_B, (y >> 6) & 1) ;
  digitalWrite(MY_PIN_8S_C, (y >> 5) & 1) ;
  digitalWrite(MY_PIN_8S_D, (y >> 4) & 1) ;
  digitalWrite(MY_PIN_8S_E, (y >> 3) & 1) ;
  digitalWrite(MY_PIN_8S_F, (y >> 2) & 1) ;
  digitalWrite(MY_PIN_8S_G, (y >> 1) & 1) ;
}

void my8SRenderNumber ()
{
  // 1st digit

  my8SRenderDigit(my_8s_num[0]) ;
  
  digitalWrite(MY_PIN_8S_0, E_8S) ;
  delay(1) ;
  digitalWrite(MY_PIN_8S_0, D_8S) ;
  
  // 2nd digit
  
  my8SRenderDigit(my_8s_num[1]) ;
  
  digitalWrite(MY_PIN_8S_1, E_8S) ;
  delay(1) ;
  digitalWrite(MY_PIN_8S_1, D_8S) ;
  
  // 3rd digit
  
  my8SRenderDigit(my_8s_num[2]) ;
  
  digitalWrite(MY_PIN_8S_2, E_8S) ;
  delay(1) ;
  digitalWrite(MY_PIN_8S_2, D_8S) ;
  
  // 4th digit
  
  my8SRenderDigit(my_8s_num[3]) ;
  
  digitalWrite(MY_PIN_8S_3, E_8S) ;
  delay(1) ;
  digitalWrite(MY_PIN_8S_3, D_8S) ;
  
  delay(26) ;
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

bool myRegisterShiftIn (int sh)
{
  if (!myRegisterIsLocked())
    return false ;
  
  // read data from the register
  
  unsigned char x = 0 ;
  
  Serial.print("*** Register contained 0B") ;
  for (int i = 7 ; i >= 0 ; --i) {
    int b = (unsigned char)digitalRead(MY_PIN_VALUE) ;
    Serial.print(b) ;
    x |= b << i ;
    digitalWrite(MY_PIN_CLOCK , HIGH) ;
    digitalWrite(MY_PIN_CLOCK ,  LOW) ;
  }
  Serial.print(" ***\n") ;
  
  digitalWrite(MY_PIN_VALUE, LOW) ;
  
  my_value |= x << sh ;
  
  // unlock the register
  myRegisterUnlock() ;
  
  return true ;
}
