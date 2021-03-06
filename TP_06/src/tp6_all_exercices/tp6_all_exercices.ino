#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#define MAX_WAIT_FOR_TIMER 5

int cpt = 0;

unsigned int waitFor(int timer, unsigned long period){
  static unsigned long waitForTimer[MAX_WAIT_FOR_TIMER];
  unsigned long newTime = micros() / period;              // numéro de la période modulo 2^32 
  int delta = newTime - waitForTimer[timer];              // delta entre la période courante et celle enregistrée
  if ( delta < 0 ) delta += 1 + (0xFFFFFFFF / period);    // en cas de dépassement du nombre de périodes possibles sur 2^32 
  if ( delta ) waitForTimer[timer] = newTime;             // enregistrement du nouveau numéro de période
  return delta;
}

//--------- définition de la tache Led

struct Led_st {
  int timer;                                              // numéro de timer utilisé par WaitFor
  unsigned long period;                                   // periode de clignotement
  int pin;                                                // numéro de la broche sur laquelle est la LED
  int etat;                                               // etat interne de la led
}; 

void setup_Led(struct Led_st* ctx, int timer, unsigned long period, byte pin) {
  ctx->timer = timer;
  ctx->period = period;
  ctx->pin = pin;
  ctx->etat = 0;
  pinMode(pin,OUTPUT);
  digitalWrite(pin, ctx->etat);
}

void loop_Led(struct Led_st* ctx) {
  if (!waitFor(ctx->timer, ctx->period)) return;          // sort s'il y a moins d'une période écoulée
  digitalWrite(ctx->pin,ctx->etat);                       // ecriture
  ctx->etat = 1 - ctx->etat;                              // changement d'état
}

//--------- definition de la tache Mess

struct Mess_st {
  int timer;                                              // numéro de timer utilisé par WaitFor
  unsigned long period;                                   // periode d'affichage
  char mess[20];
}; 

void setup_Mess(struct Mess_st * ctx, int timer, unsigned long period, const char * mess) {
  ctx->timer = timer;
  ctx->period = period;
  strcpy(ctx->mess, mess);
  Serial.begin(9600);                                     // initialisation du débit de la liaison série
}

void loop_Mess(struct Mess_st *ctx) {
  if (!(waitFor(ctx->timer,ctx->period))) return;         // sort s'il y a moins d'une période écoulée
  Serial.println(ctx->mess);                              // affichage du message
}

//--------- definition de la tache Compteur

struct Compteur_st {
   int timer;
   unsigned long period;
};

void setup_Compteur(struct Compteur_st * ctx, int timer, unsigned long period) {
  ctx->timer = timer;
  ctx->period = period;

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.display();
  delay(2000);

  display.clearDisplay(); 
  Serial.begin(9600);
}

void loop_Compteur(struct Compteur_st * ctx) {
  if (!(waitFor(ctx->timer,ctx->period))) return;         // sort s'il y a moins d'une période écoulée
  cpt++;
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(cpt, DEC);
  display.display();
  display.clearDisplay();
}

//--------- definition de la tache Boite au letttre

struct mailbox {  enum {EMPTY, FULL} state;
  int timer;
  unsigned long period;
  unsigned long val;
} mb0 = {.state = mailbox::EMPTY};

void setup_T(struct mailbox *ctx, int timer, unsigned long period) {
  ctx->timer = timer;
  ctx->period = period;
  Serial.begin(9600); 
}

// Ecriture
void loop_T1(struct mailbox *ctx, int pin) {  
  if (ctx->state != mailbox::EMPTY) return;         // attend que la mailbox soit vide
  ctx->val = analogRead(pin);
  ctx->state = mailbox::FULL;
}

// Lecture 
void loop_T2(struct mailbox* ctx, struct Led_st* led) {
  if (!waitFor(ctx->timer, ctx->period)) return;    // sort s'il y a moins d'une période écoulée
  led->period = (ctx->val)*1000;
  if (ctx->state != mailbox::FULL) return;          // attend que la mailbox soit pleine
  ctx->state = mailbox::EMPTY;
}

//--------- Déclaration des tâches

struct Mess_st Mess1;
struct Mess_st Mess2;
struct Compteur_st Cpt1;
struct mailbox Mb1;
struct Led_st Led1;

//--------- Setup et Loop

void setup() {
  setup_Mess(&Mess1, 0, 1500000, "Bonjour");              // Mess est exécutée toutes les 1.5 secondes 
  setup_Mess(&Mess2, 1, 1500000, "Salut");                // Mess est exécutée toutes les 1.5 secondes 
  setup_Compteur(&Cpt1, 2, 1000000);                      // Cpt est exécutée toutes les 500 ms
  setup_T(&Mb1, 3, 500000);                          // Mb est exécutée toutes les 500ms 
  setup_Led(&Led1, 4, 500000, 13);                  // Led est exécutée toutes les 500ms 
}

void loop() {
  loop_Mess(&Mess1); 
  loop_Mess(&Mess2); 
  loop_Compteur(&Cpt1);
  loop_T1(&Mb1, 15);
  loop_T2(&Mb1, &Led1);
  loop_Led(&Led1); 
}
