#include <IRremote.h> 

#define TRANSMITTER_PIN  18 
#define TESTBUTTON_PIN  27
#define PIR_PIN  34
#define OBSTACLE_PIN 25 

#define GREEN_LED 32
#define RED_LED 13

#define PHOTORESISTOR_PIN 33  

#define LAMP_ADDRESS  0xEF00
#define CMD_ON        0x00
#define CMD_OFF       0x01

// stati
enum Stato {
  IDLE,
  ENTRATA,
  USCITA, 
  ATTESA_CHIUSURA
};

Stato statoCorrente = IDLE;

// Indicativo dell'ultimo segnale dato alla lampada
bool luceAccesa = true; 
const int LIGHT_THRESHOLD = 1500;

unsigned long statoStartTime = 0;
const int TIMEOUT = 1000 * 10; // 10 secondi

// Controllo periodico luminosità
unsigned long lastLightCheck = 0;
const int LIGHT_CHECK_INTERVAL = 10000; // ogni 10 secondi

// Cooldown PIR 
unsigned long pirCooldownStart = 0;
const int PIR_COOLDOWN_TIME = 5000; // 5 secondi

// persone nella stanza (parte da 1)
int persone = 1;   

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(TESTBUTTON_PIN, INPUT_PULLUP); 

  IrSender.begin(TRANSMITTER_PIN);

  pinMode(PIR_PIN, INPUT); 
  pinMode(RED_LED, OUTPUT); 
  pinMode(OBSTACLE_PIN, INPUT); 
  pinMode(PHOTORESISTOR_PIN, INPUT); 
  
  Serial.print("Luminosità ambiente: "); 
  Serial.println(analogRead(PHOTORESISTOR_PIN)); 
}

void loop() {

  // Reset manuale 
  if (digitalRead(TESTBUTTON_PIN) == LOW) {
    persone = 1;
    luceAccesa = true; 
    Serial.println("Reset manuale, persone = 1"); 
    delay(200); 
    checkAndControlLight(); 
    while (digitalRead(TESTBUTTON_PIN) == LOW);
  }

  ////////
  // LETTURA SENSORI
  
  bool motionDetected = digitalRead(PIR_PIN);
  digitalWrite(RED_LED, motionDetected);

  bool doorOpen = digitalRead(OBSTACLE_PIN); // LOW = chiusa, HIGH = aperta
  int lightLevel = analogRead(PHOTORESISTOR_PIN);

  ////////
  // CONTROLLO DELLO STATO 
  
  switch (statoCorrente) {

    case IDLE:
      {
        // Ignora PIR se siamo in cooldown
        /*
          dato che il pir module scatta ad ogni variazione di temperaratura, avremo 2 trigger del pir 
          quando la persona si posiziona davanti e quando esce. (a volte... non sempre accade)

          vogliamo ignorare il trigger dovuto alla discesa di temperatura 
        */
        bool pirInCooldown = (millis() - pirCooldownStart < PIR_COOLDOWN_TIME);
        bool pirActive = motionDetected && !pirInCooldown;
        
        // Porta aperta -> possibile entrata
        if (doorOpen) { 
          statoCorrente = ENTRATA;
          statoStartTime = millis();
          Serial.println("Porta aperta, IDLE -> ENTRATA");
        }
        // PIR attivato (fuori dal cooldown) -> possibile uscita
        else if (pirActive) {
          // Se 0 persone e PIR trigger, qualcuno è rimasto dentro!
          if (persone == 0) {
            persone = 1;
            Serial.println("PIR con 0 persone -> persone = 1");
            checkAndControlLight();
          } else {
            statoCorrente = USCITA;
            statoStartTime = millis();
            Serial.println("PIR rilevato, IDLE -> USCITA");
          }
        }
      }
      break;

    case ENTRATA:
      // Ingresso confermato (PIR attivo dopo porta aperta)
      if (motionDetected) {
        persone++;
        Serial.print("Ingresso confermato, persone = ");
        Serial.println(persone);
        Serial.println("ENTRATA -> ATTESA_CHIUSURA");
        statoCorrente = ATTESA_CHIUSURA;
        checkAndControlLight();
      }
      else if (millis() - statoStartTime > TIMEOUT || !doorOpen) {
        Serial.println("Timeout ingresso, ENTRATA -> IDLE");
        statoCorrente = IDLE;
      }
      break;

    case USCITA:
      // Porta aperta dopo PIR -> uscita confermata
      if (doorOpen) {
        persone--;
        if (persone < 0) persone = 0;
        Serial.print("Uscita confermata, persone = ");
        Serial.println(persone);
        statoCorrente = ATTESA_CHIUSURA;
        statoStartTime = millis();
        checkAndControlLight();
        Serial.println("USCITA -> ATTESA_CHIUSURA");
      }
      else if (millis() - statoStartTime > TIMEOUT) {
        Serial.println("Timeout uscita, USCITA -> IDLE");
        statoCorrente = IDLE;
      }
      break;

    case ATTESA_CHIUSURA:
      // Attendo che la porta si richiuda
      if (!doorOpen) {
        statoCorrente = IDLE;
        statoStartTime = millis();
        pirCooldownStart = millis(); // Avvia cooldown PIR
        Serial.println("Porta chiusa, ATTESA_CHIUSURA -> IDLE");
      } 
      break;
  }

  // Controllo periodico della luce
  if (millis() - lastLightCheck > LIGHT_CHECK_INTERVAL) {
    lastLightCheck = millis();
    checkAndControlLight();
  }
}


void checkAndControlLight() {
  int lightLevel = analogRead(PHOTORESISTOR_PIN);
  
  Serial.print("Luminosità: ");
  Serial.print(lightLevel);
  Serial.print(" | Persone: ");
  Serial.println(persone);

  // Se la luce non è accesa
  if (!luceAccesa) {
    // Non accendere se la soglia non è superata
    if (lightLevel < LIGHT_THRESHOLD) {
      return; 
    } else { 
      if (persone > 0) accendi(); 
    }
  } else {
    // Se la luce è accesa ma ci sono 0 persone, spegni
    if (persone <= 0) {
      spegni(); 
    } else {
      accendi(); 
    }
  }
}

void spegni() {
  digitalWrite(GREEN_LED, LOW);
  for (int i = 0; i < 6; i++) {
    IrSender.sendNEC(LAMP_ADDRESS, CMD_OFF, 0);
  }
  luceAccesa = false;
  Serial.println("Lampada OFF");
}


void accendi() {
  digitalWrite(GREEN_LED, HIGH);
  for (int i = 0; i < 6; i++) {
    IrSender.sendNEC(LAMP_ADDRESS, CMD_ON, 0);
  }
  luceAccesa = true;
  Serial.println("Lampada ON");
}




  //CODICE PER RICEVERE IL SEGNALE
  // if (IrReceiver.decode()) {
  //     IrReceiver.printIRResultShort(&Serial);
  //     IrReceiver.resume();
  //   }


  //receiver setup (da togliere una volta che si è registrato tutto)
  //IrReceiver.begin(receiver, ENABLE_LED_FEEDBACK);
  //Serial.println("ricevitore ok");



