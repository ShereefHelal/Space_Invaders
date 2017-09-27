// SpaceInvaders.c
// Runs on LM4F120/TM4C123
// This is a starter project for the edX Lab 15
// In order for other students to play your game
// 1) You must leave the hardware configuration as defined
// 2) You must not add/remove any files from the project
// 3) You must add your code only this this C file
// I.e., if you wish to use code from sprite.c or sound.c, move that code in this file
// 4) It must compile with the 32k limit of the free Keil


// Author: Shereef Helal
// August 26, 2017
// This is implementation of 1980s-style SpaceInvaders game


// ******* Required Hardware I/O connections*******************
// Slide pot pin 1 connected to ground
// Slide pot pin 2 connected to PE2/AIN1
// Slide pot pin 3 connected to +3.3V 
// fire button connected to PE0
// special weapon fire button connected to PE1
// 8*R resistor DAC bit 0 on PB0 (least significant bit)
// 4*R resistor DAC bit 1 on PB1
// 2*R resistor DAC bit 2 on PB2
// 1*R resistor DAC bit 3 on PB3 (most significant bit)
// LED on PB4
// LED on PB5

// Blue Nokia 5110
// ---------------
// Signal        (Nokia 5110) LaunchPad pin
// Reset         (RST, pin 1) connected to PA7
// SSI0Fss       (CE,  pin 2) connected to PA3
// Data/Command  (DC,  pin 3) connected to PA6
// SSI0Tx        (Din, pin 4) connected to PA5
// SSI0Clk       (Clk, pin 5) connected to PA2
// 3.3V          (Vcc, pin 6) power
// back light    (BL,  pin 7) not connected, consists of 4 white LEDs which draw ~80mA total
// Ground        (Gnd, pin 8) ground

// Red SparkFun Nokia 5110 (LCD-10168)
// -----------------------------------
// Signal        (Nokia 5110) LaunchPad pin
// 3.3V          (VCC, pin 1) power
// Ground        (GND, pin 2) ground
// SSI0Fss       (SCE, pin 3) connected to PA3
// Reset         (RST, pin 4) connected to PA7
// Data/Command  (D/C, pin 5) connected to PA6
// SSI0Tx        (DN,  pin 6) connected to PA5
// SSI0Clk       (SCLK, pin 7) connected to PA2
// back light    (LED, pin 8) not connected, consists of 4 white LEDs which draw ~80mA total



#include "..//tm4c123gh6pm.h"
#include "Nokia5110.h"
#include "Random.h"
#include "TExaS.h"
#include "Global variables.h"

//All objects in the game has the same attributes in this struct
struct State{
  unsigned long x;      // x coordinate
  unsigned long y;      // y coordinate
  const unsigned char *image[2]; // two pointers to images
  long life;            // 0=dead, 1=alive
};          
typedef struct State STyp;
STyp Enemy[5];         //Max of 5 enemies at the same time
STyp Player_Ship;      //One player ship
STyp Shoots[10];       //Max of 10 enemy shoots at the same time
STyp Ship_Shoots[10];  //Max of 10 ship shoots at the same time
STyp Missiles;         //One missile at a time

//Print welcome message
void Welcome_Message(void){
	Nokia5110_Clear();
  Nokia5110_SetCursor(4, 0);
  Nokia5110_OutString("Space");
	Nokia5110_SetCursor(2, 1);
  Nokia5110_OutString("Invaders");
  
  Nokia5110_SetCursor(2, 3);
  Nokia5110_OutString("Press any");
	Nokia5110_SetCursor(0, 4);
  Nokia5110_OutString("key to start");
	}
	
	//initialize DAC and LEDs
	void DAC_Init(void){
  volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x00000002;      // 1) activate port B
  delay = SYSCTL_RCGC2_R;            // 2) allow time to finish activating
	GPIO_PORTB_CR_R = 0x3F;            // 3) allow changes to PB5-0
  GPIO_PORTB_AMSEL_R = 0x00;         // 4) disable analog on PB
  GPIO_PORTB_PCTL_R = 0x00000000;    // 5) PCTL GPIO on PB5-0
  GPIO_PORTB_DR8R_R = 0x3F;          // 6) Set PortB max current to 8mA
  GPIO_PORTB_DIR_R = 0x3F;	         // 7) PB5-0 out
  GPIO_PORTB_AFSEL_R = 0x00;         // 8) disable alt funct on PB
  GPIO_PORTB_DEN_R = 0x3F;           // 9) enable digital I/O on PB5-0
}


// **************DAC_Out*********************
// output to DAC
// Input: 4-bit data, 0 to 15 
// Output: none
void DAC_Out(unsigned long data){
  DAC = data ;
}
	
 //Timer2A interrupt initialization
	void Timer2_Init(unsigned long period){ 
		
  unsigned long volatile delay;
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  delay = SYSCTL_RCGCTIMER_R; 
  TimerCount = 0;
  Semaphore = 0;
  TIMER2_CTL_R = 0x00000000;    // 1) disable timer2A during setup
  TIMER2_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period - 1;  // 4) reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R = 0x00000001;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x80000000; // 8) priority 4
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 23 in NVIC
}
	

//Play any sound loaded to the wave sound array
void Play(void){
  if(count != 0){
    DAC_Out(Wave[Index]>>4);
    Index = Index + 1;
    count = count - 1;
  }else{
  NVIC_DIS0_R = 1<<19;           // disable IRQ 19 in NVIC
  }
}


void Sound_Init(void){
  DAC_Init();               // initialize simple 4-bit DAC
  Index = 0;
  count = 0;
}

//Pass the sound array name and number of elements to output a particular tune
void Sound_Play(const unsigned char *pt, unsigned long countv){
  Wave = pt;
  Index = 0;
  count = countv;
  NVIC_EN0_R = 1<<19;           // 9) enable IRQ 19 in NVIC
  TIMER2_CTL_R = 0x00000001;    // 10) enable TIMER2A after setting the sound values
}

//functions to output different sounds
void Sound_Shoot(void){
  Sound_Play(shoot,4080);
}
void Sound_Killed(void){
  Sound_Play(invaderkilled,3377);
}
void Sound_Explosion(void){
  Sound_Play(explosion,2000);
}

void Sound_Highpitch(void){
  Sound_Play(highpitch,1802);
}


// This initialization function sets up the ADC & switches
// Max sample rate: <=125,000 samples/second
// SS3 triggering event: software trigger
// SS3 1st sample source:  channel 1
// SS3 interrupts: enabled but not promoted to controller
void ADC0_Init(void){ 
	
	volatile unsigned long delay;
  SYSCTL_RCGC2_R |= 0x00000010;   // 1) activate clock for Port E
  delay = SYSCTL_RCGC2_R;         //    allow time for clock to stabilize
  GPIO_PORTE_DIR_R &= ~0x07;      // 2) make PE2-0 input (for switches)
  GPIO_PORTE_AFSEL_R |= 0x04;     // 3) enable alternate function on PE2
  GPIO_PORTE_DEN_R &= ~0x04;      // 4) disable digital I/O on PE2
  GPIO_PORTE_AMSEL_R |= 0x04;     // 5) enable analog function on PE2
  SYSCTL_RCGC0_R |= 0x00010000;   // 6) activate ADC0
  delay = SYSCTL_RCGC2_R;        
  SYSCTL_RCGC0_R &= ~0x00000300;  // 7) configure for 125K
  ADC0_SSPRI_R = 0x0123;          // 8) Sequencer 3 is highest priority
  ADC0_ACTSS_R &= ~0x0008;        // 9) disable sample sequencer 3
  ADC0_EMUX_R &= ~0xF000;         // 10) seq3 is software trigger
  ADC0_SSMUX3_R &= ~0x000F;       // 11) clear SS3 field
  ADC0_SSMUX3_R += 1;             //    set channel Ain1 (PE2)
  ADC0_SSCTL3_R = 0x0006;         // 12) no TS0 D0, yes IE0 END0
  ADC0_ACTSS_R |= 0x0008;         // 13) enable sample sequencer 3
	
	GPIO_PORTE_DEN_R |= 0x03;       //  enable digital I/O on PE1-0
	GPIO_PORTE_AMSEL_R &= ~0x03;    //  disable analog on PE1-0
  GPIO_PORTE_PCTL_R &= ~0x03;     //  PCTL GPIO on PE1-0
	GPIO_PORTE_AFSEL_R &= ~0x03;    //  disable alt funct on PE1-0
  
}

//Reads the value of the ADC and return its value
unsigned long ADC0_In(void){ unsigned long result ; 
   ADC0_PSSI_R = 0x0008;           // 1) initiate SS3
  while((ADC0_RIS_R&0x08)==0){};   // 2) wait for conversion done
  result = ADC0_SSFIFO3_R&0xFFF;   // 3) read result
  ADC0_ISC_R = 0x0008;             // 4) acknowledge completion

		return result ;
}


void Systic_Interrupt_init(unsigned long period){
  
  NVIC_ST_CTRL_R = 0;             // disable SysTick during setup
  NVIC_ST_RELOAD_R = period-1;    // reload value
  NVIC_ST_CURRENT_R = 0;          // any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x20000000;  // priority 1
  NVIC_ST_CTRL_R = 0x0007;        // enable,core clock, and interrupts
}


// Adds the attributes of 10 enemy shoots and 10 ship shoots, 1 missile and player ship
void Game_Engine_Init(void){int i;
	
	for(i=0;i<10;i++){
		//Enemy shoots
    Shoots[i].x = 1;   //Init x pos
    Shoots[i].y = 1;   //Init y pos
    Shoots[i].image[0] = Laser0;   //Laser image
    Shoots[i].image[1] = Missile1; //Blank space to demise laser
    Shoots[i].life = 0;
		//Ship shoots
		Ship_Shoots[i].x = 1;  //Init x pos
    Ship_Shoots[i].y = 1;  //Init y pos
    Ship_Shoots[i].image[0] = Missile0;  //Shoot image
    Ship_Shoots[i].image[1] = Missile1;  //Blank space to demise shoot
    Ship_Shoots[i].life = 0;
   }
	  //Missile
	  Missiles.x = 1;   //Init x pos   
	  Missiles.y = 1;   //Init y pos
	  Missiles.image[0] = Missile;   //Missile image
    Missiles.image[1] = MissileC;  //Blank space to demise missile
    Missiles.life = 0;  
	  //Player ship
	 	Player_Ship.x = 34;  //Init x pos
    Player_Ship.y = 47;  //Init y pos
    Player_Ship.image[0] = PlayerShip0;
    Player_Ship.life = 1;
}


void Level1_Init(void){ int i;
	Enemy_Alive=5;              //start with 5 alive enemies
	Enemy_Count=5;              //5 more enemies can be created
	Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(3, 2);
  Nokia5110_OutString("Level 1");
	Delay100ms(5) ;
	
	Nokia5110_ClearBuffer();
	//Add attributes of 5 enemies
  for(i=0;i<5;i++){
    Enemy[i].x = 16*i + 2;   //Distribute enemies evenly horizontally
    Enemy[i].y = 10;         //All enemies have the same y pos
    Enemy[i].image[0] = SmallEnemy30PointA; //Is killed by one hit
    Enemy[i].image[1] = SmallEnemy30PointB;
    Enemy[i].life = 1;  
   }
	 //Show enemies and player ship on the screen
	 for(i=0;i<5;i++){
		 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0 ) ;
	 }
	 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	 Nokia5110_DisplayBuffer(); 
}


void Level2_Init(void){ int i;
	Enemy_Alive=5;              //start with 5 alive enemies
	Enemy_Count=5;              //5 more enemies can be created
	Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(3, 2);
  Nokia5110_OutString("Level 2");
	Delay100ms(5) ;
	
	  Nokia5110_ClearBuffer();
	//Add attributes of 5 enemies randomly
  for(i=0;i<5;i++){
		rand1 = Random32()%8;   //Creats a random number from 0-7
    Enemy[i].x = 16*i + 2;
    Enemy[i].y = 10;
		//Creates one enemy randomly from two different types
    if(rand1==0 || rand1==1 || rand1==5 || rand1==7){
		Enemy[i].image[0] = SmallEnemy30PointA; //Is killed by one hit
    Enemy[i].image[1] = SmallEnemy30PointB;
    Enemy[i].life = 1; 
		}
   else {
		Enemy[i].image[0] = SmallEnemy20PointA; //Is killed by two hits
    Enemy[i].image[1] = SmallEnemy20PointB;
    Enemy[i].life = 2; 
	 }
	}
	
	 for(i=0;i<5;i++){
		 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0 ) ;
	 }
	 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	 
	 Nokia5110_DisplayBuffer(); 
}

//Follows the same logic of previous levels
void Level3_Init(void){ int i;
	Enemy_Alive=5;              //start with 5 alive enemies
	Enemy_Count=5;              //5 more enemies can be created
	Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(3, 2);
  Nokia5110_OutString("Level 3");
	Delay100ms(5) ;
	
	  Nokia5110_ClearBuffer();
	
  for(i=0;i<5;i++){
		rand1 = Random32()%8;
    Enemy[i].x = 16*i + 2;
    Enemy[i].y = 10;
    if(rand1==0 || rand1==1 || rand1==5 || rand1==7){
		Enemy[i].image[0] = SmallEnemy20PointA;  //Is killed by two hits
    Enemy[i].image[1] = SmallEnemy20PointB;
    Enemy[i].life = 2; 
		}
   else {
		Enemy[i].image[0] = SmallEnemy10PointA;  //Is killed by three hits
    Enemy[i].image[1] = SmallEnemy10PointB;
    Enemy[i].life = 3; 
	 }
	}
	 
	 for(i=0;i<5;i++){
		 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0 ) ;
	 }
	 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	 
	 Nokia5110_DisplayBuffer();	 
}

//Follows the same logic of previous levels
void Level4_Init(void){ int i;
	Enemy_Alive=5;              //start with 5 alive enemies
	Enemy_Count=5;              //5 more enemies can be created
	Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(3, 2);
  Nokia5110_OutString("Level 4");
	Delay100ms(5) ;
	
	  Nokia5110_ClearBuffer();
	
  for(i=0;i<5;i++){
		rand1 = Random32()%8;
    Enemy[i].x = 16*i + 2;
    Enemy[i].y = 10;
    if(rand1==0 || rand1==1 || rand1==5 || rand1==7){
		Enemy[i].image[0] = SmallEnemy10PointA;  //Is killed by three hits
    Enemy[i].image[1] = SmallEnemy10PointB;
    Enemy[i].life = 3; 
		}
   else {
		Enemy[i].image[0] = SmallEnemynew;       //Is killed by four hits
    Enemy[i].image[1] = SmallEnemynewB;
    Enemy[i].life = 4; 
	 }
	}
		 
	 for(i=0;i<5;i++){
		 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0 ) ;
	 }
	 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	 
	 Nokia5110_DisplayBuffer();
}


void Boss_Init(void){ int i;
	Enemy_Alive=3;     //start with 3 alive enemies
	Enemy_Count=50000; //enemies can be created until boss dies and the player wins
	Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(3, 1);
  Nokia5110_OutString("Level 5");
	Delay100ms(5) ;
	Nokia5110_SetCursor(0, 3);
	Nokia5110_OutString(" KILL BOSS!!");
	Delay100ms(5) ;
	
	Nokia5110_ClearBuffer();
	
	//Reset all shoots status
	ship_shoots_num=0;
	shoots_num=0;
	for(i=0;i<10;i++){
  Shoots[i].life = 0;
  Ship_Shoots[i].life = 0;
 }
		//Creats two minions and the boss
    Enemy[0].x = 3;
    Enemy[0].y = 10;
    Enemy[0].image[0] = SmallEnemy30PointA;  //Is killed by one hit
    Enemy[0].image[1] = SmallEnemy30PointB;
    Enemy[0].life = 1;
	
	  Enemy[1].x = 67;
    Enemy[1].y = 10;
    Enemy[1].image[0] = SmallEnemy30PointA;  //Is killed by one hit
    Enemy[1].image[1] = SmallEnemy30PointB;
    Enemy[1].life = 1;
		
		Enemy[2].x = 20;
    Enemy[2].y = 21;
    Enemy[2].image[0] = BossA;    //Is killed by thirty hits
    Enemy[2].image[1] = BossB;
    Enemy[2].life = 30;
	 
 //Prints ship, minions and the boss
	 for(i=0;i<3;i++){
		 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0 ) ;
	 }
	 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	 Nokia5110_PrintBMP(1, 38, HP, 0) ;
	 Nokia5110_DisplayBuffer();
}

//Moves any object of STyp one step to right
void Move_Right(STyp *A){  
	  (*A).x = (*A).x+ 1;
}

//Moves any object of STyp one step to left
void Move_Left(STyp *A){  
	  (*A).x = (*A).x- 1; 
}

//Moves one random enemy one step to left or right
void Move_Enemy(void){
	
	  rand0 = Random32()%15;  //Creates a random number from 0-14
		rand1 = Random32()%5;   //Creates a random number from 0-4
	
    if(rand0 == 0 && (right[rand1]==0) && Enemy[rand1].life>=1) { //There is 1/15 chance that the condition is true
		Move_Right(&Enemy[rand1]); //Moves a random enemy to right
		Nokia5110_PrintBMP(Enemy[rand1].x, Enemy[rand1].y, Enemy[rand1].image[shape[rand1]], 0 ) ;
		shape[rand1] ^= 1; //Change the shape of an enemy when it moves
		Nokia5110_DisplayBuffer();
		
		right[rand1]=1;  //The enemy has moved to right
		left[rand1]=0;   //The enemy hasn't moved to left 
		}
		if(rand0 == 1 && (left[rand1]==0)&& Enemy[rand1].life>=1) {  //There is 1/15 chance that the condition is true
		Move_Left(&Enemy[rand1]);  //Moves a random enemy to left
		Nokia5110_PrintBMP(Enemy[rand1].x, Enemy[rand1].y, Enemy[rand1].image[shape[rand1]], 0 ) ;
		shape[rand1] ^= 1;  //Change the shape of an enemy when it moves
		Nokia5110_DisplayBuffer();
		
		right[rand1]=0;  //The enemy has moved to left
		left[rand1]=1;   //The enemy hasn't moved to right
		}
}

//Moves player ship according to the position of the slide potentiometer
void Move_Ship(void){
	
 if((Player_Ship.x < Slide) && (Player_Ship.life==1)) {
		 Player_Ship.x+=2;
		 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	   Nokia5110_DisplayBuffer();
	 }
		if((Player_Ship.x > Slide+1) && (Player_Ship.life==1)) {
		 Player_Ship.x= Player_Ship.x-2;
		 Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, Player_Ship.image[0], 0) ;
	   Nokia5110_DisplayBuffer();
		 
	 }
}

//Destroys the player ship on getting hit
void Kill_Player (void){ int i;
	//Checks if any of the existing shoots hit the player ship
	for(i=0 ; i<10; i++) { if((Shoots[i].life == 1) && (Shoots[i].y >= 43) && (Shoots[i].x>= (Player_Ship.x)-1) && (Shoots[i].x<= (Player_Ship.x)+15)) {
			Shoots[i].life = 0;  //End shoot  
			shoots_num-- ;       //Number of existing shoots is decreased
		  Lives -- ;           //Number of player lives is decreased
		  Sound_Explosion();   //Plays explosion sound
			Nokia5110_PrintBMP(Shoots[i].x, Shoots[i].y, Laser1, 0 );  //Clear shoot image
		  //Display explosion of the ship
		  Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, BigExplosion0, 0);  
			Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, BigExplosion1, 0);
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, PlayerShip1, 0);
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  //If player has lives left, display the ship again
		  if(Lives>0) {
			Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, PlayerShip0, 0);
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
      Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, PlayerShip1, 0) ;
	 	  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  Nokia5110_PrintBMP(Player_Ship.x, Player_Ship.y, PlayerShip0, 0) ;
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
			}
		}
	 }
	}

	
	void Kill_Enemy (void){ int i;
		
		for(i=0; i<5; i++){  //starts a counter when an enemy is destroyed
			if(Enemy_Destroyed[i]>=1) {
			   Enemy_Destroyed[i]++;
		}
		if(Enemy_Destroyed[i]==15) {  //is true after 15 bus cycles (0.5s)
			 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, SmallExplosion1, 0); //clears enemy from the screen
			 Nokia5110_DisplayBuffer();
			 Enemy_Destroyed[i]=0;   //resets counter
			 Enemy_Alive--;          //Number of enemies alive is decreased
			 Score+=20;              
			 Lives_Buffer+=20;
			 Missiles_Buffer+=20;
			 }
			
			 if(Enemy[i].life==0 && Flag0[i]==1) {  //is true when the flag of enemy killed is set
			 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, SmallExplosion0, 0);  //displays explosion on the screen
			 Nokia5110_DisplayBuffer();
		   Enemy_Destroyed[i]=1;  //indicates the enemy explosion
			 Sound_Killed();        //plays explosion sound
			 Flag0[i]=0;            //clears the flag
			 }
		}
	}
	
	//Checks if any ship shoot hit an enemy
	//Enemy life is decreased by 1 when it is hit
	void Damage_Enemy (void){ int i,j;
	
	for(i=0 ; i<10; i++){ 
		 for(j=0; j<5; j++){
		
		if((Ship_Shoots[i].life == 1) && (Ship_Shoots[i].y <= 16) && (Ship_Shoots[i].x>= Enemy[j].x) && (Ship_Shoots[i].x<= Enemy[j].x+11) && (Enemy[j].life>0)) {
			Enemy[j].life--;
		  Score+=10;
			Lives_Buffer+=10;
			Missiles_Buffer+=10;
		  Ship_Shoots[i].life = 0 ;
			ship_shoots_num-- ;
			Nokia5110_PrintBMP(Ship_Shoots[i].x, Ship_Shoots[i].y, Missile2, 0 ) ;
		  Nokia5110_DisplayBuffer();
		  if(Enemy[j].life==0) {Flag0[j]=1;}  //sets the enemy killed flag
	 }
	}
 }
}
	

	void Creat_Enemy_Shoots(void){
	   rand0 = Random32()%11;
		 rand1 = Random32()%5;
    if(rand0 == 0  && Shoots[shoots_num].life==0 && Enemy[rand1].life>=1){ //Has 1/11 chance to be true 
		Shoots[shoots_num].x = (Enemy[rand1].x)+7;
		Shoots[shoots_num].y = (Enemy[rand1].y)+8;
		Shoots[shoots_num].life = 1;
		Nokia5110_PrintBMP(Shoots[shoots_num].x, Shoots[shoots_num].y, Shoots[shoots_num].image[0], 0); //displays shoot on the screen
		Nokia5110_DisplayBuffer();
		 shoots_num++;	//increases the number of existing shoots
		}
	}
	
	
	unsigned long shoots_count=0;
	void Creat_Ship_Shoots(void){
	  shoots_count=(shoots_count+1)%4;
    if(A==1 && ship_shoots_num<10 && shoots_count==0){ //True each 4 bus cycles (0.23s) to avoid shoots overloading 
		Sound_Shoot();
		Ship_Shoots[ship_shoots_num].x = (Player_Ship.x)+8;
		Ship_Shoots[ship_shoots_num].y = (Player_Ship.y)-8;
		Ship_Shoots[ship_shoots_num].life = 1;
		Nokia5110_PrintBMP(Ship_Shoots[ship_shoots_num].x, Ship_Shoots[ship_shoots_num].y, Ship_Shoots[0].image[1], 0);
		Nokia5110_DisplayBuffer();
		 ship_shoots_num++;	
		}
	}

	//Moves all existing enemy shoots one step down
	void Move_Enemy_Shoots(void){int i;
		
		for(i=0 ; i<10; i++){
		if(Shoots[i].life == 1 ){
			Shoots[i].y ++ ;
			Nokia5110_PrintBMP(Shoots[i].x, Shoots[i].y, Shoots[i].image[0], 0 ) ;
		  Nokia5110_DisplayBuffer();
		}
		if((Shoots[i].y >= 47) && (Shoots[i].life == 1)){
			Shoots[i].life = 0 ;
			shoots_num-- ;
			Nokia5110_PrintBMP(Shoots[i].x, Shoots[i].y, Laser1, 0 ) ;
			Nokia5110_DisplayBuffer();
		}
	}
}
	//Moves all existing ship shoots one step up
void Move_Ship_Shoots(void){int i;
	
		for(i=0 ; i<10; i++){
		if(Ship_Shoots[i].life == 1){
			Ship_Shoots[i].y -- ;
			Nokia5110_PrintBMP(Ship_Shoots[i].x, Ship_Shoots[i].y, Ship_Shoots[0].image[1], 0 ) ;
		  Nokia5110_DisplayBuffer();
		}
		if((Ship_Shoots[i].y <= 8) && (Ship_Shoots[i].life == 1)){
			Ship_Shoots[i].life = 0 ;
			ship_shoots_num-- ;
			Nokia5110_PrintBMP(Ship_Shoots[i].x, Ship_Shoots[i].y, Missile2, 0 ) ;
			Nokia5110_DisplayBuffer();
		}
	}
}


void Create_New_Enemies(void){ int i;
	
	for(i=0; i<5; i++) {
		rand0 = Random32()%150;
		if ((rand0==0) && (Enemy[i].life==0) && (Enemy_Count>0)){ //Each dead enemy has 1/150 chance to be created again
		Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, Enemy[i].image[0], 0);
		Nokia5110_DisplayBuffer();
		//Determines enemy life according the generated shape
		if((Enemy[i].image[0])==SmallEnemy30PointA) {Enemy[i].life=1;	}
		if((Enemy[i].image[0])==SmallEnemy20PointA) {Enemy[i].life=2;	}
		if((Enemy[i].image[0])==SmallEnemy10PointA) {Enemy[i].life=3;	}
		if((Enemy[i].image[0])==SmallEnemynew)      {Enemy[i].life=4;	}
		Enemy_Count--;  //Number of possible new enemies is decreased
		Enemy_Alive++;  //Number of enemies alive is increased
		}	
	}
}


void Launch_Missile(void){int i;

   if(B==0x02 && Missile_On==0 && Missiles_num>0){ //True when B is pressed & no other missile is on the screen
		Sound_Highpitch();   //Plays missile sound
		Missiles_num--;      //Number of missiles player can launch is decreased
		Missile_On=1;        //Indicates there is a missile on the screen
		Missiles.x = (Player_Ship.x)+8;
		Missiles.y = (Player_Ship.y)-8;
		Missiles.life = 1;
		Nokia5110_PrintBMP(Missiles.x, Missiles.y, Missiles.image[0], 0 ) ;
		Nokia5110_DisplayBuffer();
		}

if(Missiles.life==1 && Missile_On==1){
	//Moves up&left or up&right till it reaches the middle of the screen 
  if(Missiles.y >= 30 && Missile_On==1) {Missiles.y--;};  
  if(Missiles.x < 38  && Missile_On==1) {Missiles.x+=2;};
  if(Missiles.x > 39  && Missile_On==1) {Missiles.x-=2;};
 	Nokia5110_PrintBMP(Missiles.x, Missiles.y, Missiles.image[0], 0) ;
  Nokia5110_DisplayBuffer();
}

  //Missile exploads when reaching mid-screen
if((Missiles.x==38||Missiles.x==39) && Missiles.y==29 && Missiles.life==1 && Missile_On==1){
	Missiles.life=0;
  Nokia5110_PrintBMP(Missiles.x, Missiles.y, MissileX, 0 ) ;
	Missile_Exploded=1;
	
	//All existing enemies life is decreased by 5
	for(i=0; i<5; i++){
	if(Enemy[i].life>=5) {Enemy[i].life-=5;}
	if(Enemy[i].life<5 && Enemy[i].life>0){
	Enemy[i].life=0;
	Flag0[i]=1;
	 }
	}
}

if(Missile_Exploded>=1){
			 Missile_Exploded++;
		}
		if(Missile_Exploded==5){ //Clears missile explosion after 5 bus cycles
			 Missile_On=0;
			 Nokia5110_PrintBMP(Missiles.x, Missiles.y, MissileC, 0) ;
			 Nokia5110_DisplayBuffer();
			 Missile_Exploded=0;
			 }	
}


//Same logic for moving enemies but with higher rate for boss
void Move_Boss(void){
	
	   rand0 = Random32()%37;
		 rand1 = Random32()%2;
	   rand2 = Random32()%17;
	
    if(rand0 == 0 && (right[rand1]==0) && Enemy[rand1].life>=1){ 
		Move_Right(&Enemy[rand1]) ;
		Nokia5110_PrintBMP(Enemy[rand1].x, Enemy[rand1].y, Enemy[rand1].image[shape[rand1]], 0 ) ;
		shape[rand1] ^= 1;
		Nokia5110_DisplayBuffer();
		 
		right[rand1]=1;
		left[rand1]=0;
		}
		if(rand0 == 1 && (left[rand1]==0)&& Enemy[rand1].life>=1){
		Move_Left(&Enemy[rand1]) ;
		Nokia5110_PrintBMP(Enemy[rand1].x, Enemy[rand1].y, Enemy[rand1].image[shape[rand1]], 0 ) ;
		shape[rand1] ^= 1;
		Nokia5110_DisplayBuffer();
		
		right[rand1]=0;
		left[rand1]=1;
		}
		
		if(rand2 == 0 && Enemy[2].x<=22 && Enemy[2].life>=1){ 
		Move_Right(&Enemy[2]) ; 
		Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, Enemy[2].image[shape[2]], 0 ) ;
			shape[2] ^= 1;
		Nokia5110_DisplayBuffer();
		 
		}
		
		if(rand2 == 1 && Enemy[2].x>=16 && Enemy[2].life>=1){
		Move_Left(&Enemy[2]) ; 
		Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, Enemy[2].image[shape[2]], 0 ) ;
		shape[2] ^= 1;
		Nokia5110_DisplayBuffer();
		
		}	
}


//Same logic of Kill_Enemy
	void Kill_Minions (void){ int i;
		
		for(i=0; i<2; i++){	
			 if(Enemy_Destroyed[i]>=1){
			 Enemy_Destroyed[i]++;
		   }
		   if(Enemy_Destroyed[i]==15){ 
			 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, SmallExplosion1, 0) ;
			 Nokia5110_DisplayBuffer();
			 Score+=5;
			 Lives_Buffer+=5;
			 Missiles_Buffer+=5;
			 Enemy_Destroyed[i]=0;
			 }
			 if(Enemy[i].life==0 && Flag0[i]==1){
			 Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, SmallExplosion0, 0) ;
			 Nokia5110_DisplayBuffer();
		   Enemy_Destroyed[i]=1;
			 Sound_Killed();
			 Flag0[i]=0;
			 }
		}	
	}
	
	//Same logic of Damage_Enemy
	void Damage_Boss (void){ int i,j;
	
	for(i=0; i<10; i++){ 
	 for(j=0; j<2; j++){
		
		if((Ship_Shoots[i].y <= 16) && (Ship_Shoots[i].life == 1) && (Ship_Shoots[i].x>= Enemy[j].x) && (Ship_Shoots[i].x<= Enemy[j].x+11) && (Enemy[j].life>0)) {
			Enemy[j].life--;
		  Ship_Shoots[i].life = 0;
			ship_shoots_num--;
			Nokia5110_PrintBMP(Ship_Shoots[i].x, Ship_Shoots[i].y, Missile2, 0);
		  Nokia5110_DisplayBuffer();
		  if(Enemy[j].life==0) {Flag0[j]=1;}
	}
 }
	if((Ship_Shoots[i].y <= 28) && (Ship_Shoots[i].life == 1) && (Ship_Shoots[i].x>= Enemy[2].x+1) && (Ship_Shoots[i].x<= Enemy[2].x+46)) {
			Enemy[2].life--;
	    Score+=10;
			Lives_Buffer+=10;
			Missiles_Buffer+=10;
		  Ship_Shoots[i].life = 0;
			ship_shoots_num--;
			Nokia5110_PrintBMP(Ship_Shoots[i].x, Ship_Shoots[i].y, Missile2, 0 );
		  Nokia5110_DisplayBuffer();
  }	
 }
}
	
	//Same logic of Creat_Enemy_Shoots but with higher rate for boss shoots
	void Creat_Boss_Shoots(void){
	   rand0 = Random32()%57;
		 rand1 = Random32()%2;
		 
    if(rand0==5  && Shoots[shoots_num].life==0 && Enemy[rand1].life>=1) { 
		Shoots[shoots_num].x = (Enemy[rand1].x)+7;
		Shoots[shoots_num].y = (Enemy[rand1].y)+8;
		Shoots[shoots_num].life = 1;
		Nokia5110_PrintBMP(Shoots[shoots_num].x, Shoots[shoots_num].y, Shoots[shoots_num].image[0], 0 ) ;
			
		Nokia5110_DisplayBuffer();
		 shoots_num++ ;	
		}
		
		if(rand0 == 7  && Shoots[shoots_num].life==0) { 
		Shoots[shoots_num].x = (Enemy[2].x)+6;
		Shoots[shoots_num].y = (Enemy[2].y)+8;
		Shoots[shoots_num].life = 1;
		Shoots[shoots_num].image[0]=Missile0;
		Nokia5110_PrintBMP(Shoots[shoots_num].x, Shoots[shoots_num].y, Missile0, 0 ) ;
			
		Nokia5110_DisplayBuffer();
		 shoots_num++ ;	
		}
		if(rand0 == 9  && Shoots[shoots_num].life==0) { 
		Shoots[shoots_num].x = (Enemy[2].x)+38;
		Shoots[shoots_num].y = (Enemy[2].y)+8;
		Shoots[shoots_num].life = 1;
		Shoots[shoots_num].image[0]=Missile1;
		Nokia5110_PrintBMP(Shoots[shoots_num].x, Shoots[shoots_num].y, Missile1, 0 ) ;
			
		Nokia5110_DisplayBuffer();
		 shoots_num++ ;	
		}
		
		if(rand0 == 13  && Shoots[shoots_num].life==0) { 
		Shoots[shoots_num].x = (Enemy[2].x)+23;
		Shoots[shoots_num].y = (Enemy[2].y)+8;
		Shoots[shoots_num].life = 1;
		Shoots[shoots_num].image[0]=Laser0;
		Nokia5110_PrintBMP(Shoots[shoots_num].x, Shoots[shoots_num].y, Laser0, 0 ) ;
			
		Nokia5110_DisplayBuffer();
		 shoots_num++ ;	
		}
	}
	

void Create_Minions(void){ int i;
	
	for(i=0; i<2; i++) {
		rand0 = Random32()%50;
		if ((rand0==0) && (Enemy[i].life==0)) {
		Nokia5110_PrintBMP(Enemy[i].x, Enemy[i].y, SmallEnemy30PointA, 0 ) ;
		Nokia5110_DisplayBuffer();
		Enemy[i].life=1;
		}
	}
}
	
//Displays boss explosion when it is killed
void Kill_Boss(void){

if(Enemy[2].life==0){

      Score+=200;
	    Sound_Explosion();
		  Nokia5110_PrintBMP(Enemy[2].x+7, Enemy[2].y-4, SmallExplosion0, 0) ;
			Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossC, 0) ;
	    Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossA, 0) ;
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);

			Sound_Explosion();
	    Nokia5110_PrintBMP(Enemy[2].x+30, Enemy[2].y-7, SmallExplosion0, 0) ;
			Nokia5110_DisplayBuffer();
		  Delay100ms(5);
		  Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossC, 0) ;
	    Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossA, 0) ;
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
			
			Sound_Explosion();
			Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossX1, 0) ;
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
			Nokia5110_PrintBMP(Enemy[2].x, Enemy[2].y, BossC, 0) ;
		  Nokia5110_DisplayBuffer();
		  Delay100ms(5);
				
	level=10;
 }
}

//Prints winning message and score
void Congratulation(void){
	
	if(Score > High_Score) { High_Score = Score ;}
  Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(1, 0);
  Nokia5110_OutString(" You Win!");
	Nokia5110_SetCursor(0, 2);
	Nokia5110_OutString("Score ");
	Nokia5110_SetCursor(5, 2);
	Nokia5110_OutUDec(Score);
	Nokia5110_SetCursor(0, 4);
	Nokia5110_OutString("Best ");
	Nokia5110_SetCursor(5, 4);
	Nokia5110_OutUDec(High_Score);
}

//Prints game over message and score
void Game_Over(void){
	
	if(Score > High_Score) { High_Score = Score ;}
  Nokia5110_ClearBuffer();
	Nokia5110_DisplayBuffer(); 
	Nokia5110_SetCursor(1, 0);
  Nokia5110_OutString("Game Over");
	Nokia5110_SetCursor(0, 2);
	Nokia5110_OutString("Score ");
	Nokia5110_SetCursor(5, 2);
	Nokia5110_OutUDec(Score);
	Nokia5110_SetCursor(0, 4);
	Nokia5110_OutString("Best ");
	Nokia5110_SetCursor(5, 4);
	Nokia5110_OutUDec(High_Score);
}

//Increases number of lives and missiles according to gained score
void Add_Missiles_Lives(void){
	
if(Lives_Buffer >= 500) {Lives++; Lives_Buffer-=500;  }
if(Missiles_Buffer >= 400) {Missiles_num++; Missiles_Buffer-=400;  }
}

//Indicates green and red LED status according to number of lives and missiles left
void LEDs_Handler(void){

if(Lives>1) { Green_LED = 0x10; }
if(Lives==0) { Green_LED = 0; }
if(Lives==1 && Green==0) { Green=1; }
if(Green>=1) {Green++; }
if(Green==5) {Green=0; Green_LED ^= 0x10; }

if(Missiles_num>=1) { Red_LED = 0x20; }
else { Red_LED = 0; }
}


int main(void){
	
  TExaS_Init(SSI0_Real_Nokia5110_Scope);  // set system clock to 80 MHz
	
	DisableInterrupts();
  
	Random_Init(1);
  Nokia5110_Init();
	Timer2_Init(7256);     //Interrupt run at 11.025 kHz for sounds
	Sound_Init();
	ADC0_Init();
	Systic_Interrupt_init(2666667);  //Interrupt run at 30Hz for reading ADC and switches value
	Game_Engine_Init();
	
	EnableInterrupts();
  
	
  while(1){  
	
	if(level==0){
	Game_Over();
}	
	
	while(level==0 && A==0 && B==0) {;}
	Game_Engine_Init();
	//Sets initial values for the game
	Lives=5;
	level=1;
	Missiles_num=1;
	Score=0;
	Lives_Buffer=0;
	Missiles_Buffer=0;
	Green_LED = 0;
	Red_LED = 0;
	ship_shoots_num=0;
  shoots_num=0;
		
	Welcome_Message();
	Delay100ms(1);
	while (( (A||B)==0) | (Flag==0)) {Flag=0;} 
  Random_Init(NVIC_ST_CURRENT_R);

	//Remain in this loop for the first 4 levels
	while(level<5 && level>0) {
	if(level==1) {Level1_Init();}
	if(level==2) {Level2_Init();}
	if(level==3) {Level3_Init();}
	if(level==4) {Level4_Init();}
	
	while(1){ 
		while(Flag==0) {;}  //Flag is set by systic interrupt so game itself will run at 30Hz too
		
		Move_Enemy(); 
		Move_Ship(); 
    Creat_Enemy_Shoots();
		Creat_Ship_Shoots();
		Move_Enemy_Shoots();	
		Move_Ship_Shoots();
		Kill_Player();
		Damage_Enemy();
		Kill_Enemy();
		Create_New_Enemies();
		Launch_Missile();
		Add_Missiles_Lives();
		LEDs_Handler();
			
		if(Enemy_Alive==0 && Enemy_Count==0){  //Moves to next level when no more enemies can be created
		Delay100ms(5);
    level++;
		break;
		}		
	
    if(Lives==0) { level=0; break;}		
		Flag=0;
	}
}


  
	if(level==5) {Boss_Init();}
	while(level == 5){

		while(Flag==0) {;}
		
		Move_Boss(); 
		Move_Ship(); 
    Creat_Boss_Shoots();
		Creat_Ship_Shoots();
		Move_Enemy_Shoots();	
		Move_Ship_Shoots();
		Kill_Player();
		Damage_Boss();
		Kill_Minions();
		Create_Minions();
		Launch_Missile();
		Nokia5110_PrintBMP(1, 38-Enemy[2].life, Laser1, 0) ;
		Nokia5110_DisplayBuffer();
		Kill_Boss();
		Add_Missiles_Lives();
		LEDs_Handler();
			
	  if(Lives==0) { level=0;}
		Flag=0;
	}

if(level==10){
	Congratulation();
 }	
	while(level==10 && A==0 && B==0) {;}
	
 }
}


//Plays any sound loaded in the sound array at 11kHz
void Timer2A_Handler(void){ 
  TIMER2_ICR_R = 0x00000001;   // acknowledge timer2A timeout
	Play() ;
  Semaphore = 1; // trigger
}

//Reads switches and pot at 30Hz
void SysTick_Handler(void){ 
 A = GPIO_PORTE_DATA_R & 0x01; 
 B = GPIO_PORTE_DATA_R & 0x02; 
 Slide = ADC0_In()/62;
 
 Flag = 1;
}


void Delay100ms(unsigned long count){unsigned long volatile time;
  while(count>0){
    time = 727240;  // 0.1sec at 80 MHz
    while(time){
	  	time--;
    }
    count--;
  }
}


