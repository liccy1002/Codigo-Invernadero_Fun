#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>
#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
// Constants for pins, password length, and LED pins
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = { 24, 26, 28, 30 };
byte colPins[KEYPAD_COLS] = { 32, 34, 36, 38 };
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  { '1', '2', '3', '+' },
  { '4', '5', '6', '-' },
  { '7', '8', '9', '*' },
  { '.', '0', '=', '/' }
};
const int passwordLength = 4;  // Adjust password length here
const int greenLED = 13;
const int blueLED = 6;
const int redLED = 8;
const int LDR_PIN = A0;  // Pin del sensor LDR
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
char password[passwordLength + 1];  // Extra space for null terminator
char enteredPassword[passwordLength + 1];
unsigned char idx = 0;
int attempts = 0;
unsigned long lastKeypressTime = 0;
const unsigned long maxKeypressInterval = 5000;  // Adjust the interval (in milliseconds) here
#define DHTPIN 7                                 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11                            // DHT 22 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
// State Alias
enum State {
  inicio = 0,
  monitoreo = 1,
  alarma = 2,
  bloqueado = 3
};
// Input Alias
enum Input {
  Reset = 0,
  Forward = 1,
  Backward = 2,
  Unknown = 3,
};
// Create new StateMachine
StateMachine stateMachine(4, 7);
// Stores last user input
Input input;
void readtemperatura();
AsyncTask TaskTemp(2000, true, readtemperatura);
void readtemperatura() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("% Temperature: "));
  Serial.print(t);
  if (t > 40) {
    input = Forward;
  }
}
void readluz();
AsyncTask TaskLuz(1000, true, readluz);
void readluz() {
  int sensorValue = analogRead(LDR_PIN);
  Serial.print("Luz: ");
  Serial.print(sensorValue);
  if (sensorValue < 20) {
    input = Backward;
  }
}
void timeout1() {
  input = Backward;
}
AsyncTask TaskTimeout1(5000, false, timeout1);
// Setup the State Machine
void setupStateMachine() {
  // Add transitions
  stateMachine.AddTransition(inicio, monitoreo, []() {
    return input == Forward;
  });
  stateMachine.AddTransition(inicio, bloqueado, []() {
    return input == Backward;
  });
  stateMachine.AddTransition(monitoreo, alarma, []() {
    return input == Forward;
  });
  stateMachine.AddTransition(monitoreo, bloqueado, []() {
    return input == Backward;
  });
  stateMachine.AddTransition(alarma, monitoreo, []() {
    return input == Backward;
  });
  stateMachine.AddTransition(bloqueado, monitoreo, []() {
    return input == Forward;
  });
  stateMachine.AddTransition(bloqueado, inicio, []() {
    return input == Reset;
  });
  // Add actions
  stateMachine.SetOnEntering(inicio, funct_inicio);
  stateMachine.SetOnEntering(monitoreo, funct_monitoreo);
  stateMachine.SetOnEntering(alarma, funct_alarma);
  stateMachine.SetOnEntering(bloqueado, funct_bloqueado);
  stateMachine.SetOnLeaving(inicio, funct_fin_inicio);
  stateMachine.SetOnLeaving(monitoreo, funct_fin_monitoreo);
  stateMachine.SetOnLeaving(alarma, funct_fin_alarma);
  stateMachine.SetOnLeaving(bloqueado, funct_fin_bloqueado);
}
void funct_inicio(void) {
}
void funct_monitoreo(void) {
  TaskTemp.Start();
  TaskLuz.Start();
}
void funct_alarma(void) {
  TaskTimeout1.Start();
}
void funct_bloqueado(void) {
}
void funct_fin_inicio(void) {
}
void funct_fin_monitoreo(void) {
  TaskTemp.Stop();
  TaskLuz.Stop();
}
void funct_fin_alarma(void) {
}
void funct_fin_bloqueado(void) {
}
void setup() {
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  dht.begin();
  Serial.println("Starting State Machine...");
  setupStateMachine();
  Serial.println("Start Machine Started");
  // Initial state
  stateMachine.SetState(inicio, false, true);
  lcd.begin(16, 2);
  lcd.print("Ingrese clave:");
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  strcpy(password, "1234");  // Change "1234" to your desired password
}
void loop() {
  // Lee el sensor y mostrar en el lcd  como vos dices
  TaskTemp.Update();
  TaskLuz.Update();

  // Actualizar la máquina de estados como vos dices jhon gay
  //input = static_cast<Input>(readInput());
  stateMachine.Update();
  // Manejar las entradas del teclado como esta en la imagen
  if(!(input == Forward)){
  char key = keypad.getKey();
  handleKeypadInput(key);
  }
  input = Unknown;
  
}


// Auxiliar function that reads the user input
int readInput() {
  Input currentInput = Input::Unknown;
  if (Serial.available()) {
    char incomingChar = Serial.read();
    switch (incomingChar) {
      case 'R':
        currentInput = Input::Reset;
        break;
      case 'A':
        currentInput = Input::Backward;
        break;
      case 'D':
        currentInput = Input::Forward;
        break;
      default:
        break;
    }
  }
  return currentInput;
}
void handleKeypadInput(char key) {
  if (key) {
    lastKeypressTime = millis();  // Update the time of the last pressed button
    enteredPassword[idx++] = key;
    lcd.print("*");
    if (idx == passwordLength) {
      checkPassword();
      idx = 0;
      memset(enteredPassword, 0, passwordLength + 1);  // Clear password array
      lcd.clear();
      lcd.print("Ingrese clave:");
    }
  }
  // Check if the maximum time between key presses has elapsed
  if (millis() - lastKeypressTime > maxKeypressInterval && idx > 0) {
    idx = 0;
    memset(enteredPassword, 0, passwordLength + 1);  // Clear password array
    lcd.clear();
    lcd.print("Clave incorrecta");
    delay(3000);
    resetSystem();
  }
}
void resetSystem() {
  // Reset all system variables and states
  attempts = 0;
  idx = 0;
  memset(enteredPassword, 0, passwordLength + 1);
  lcd.clear();
  lcd.print("Ingrese clave:");
  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(redLED, LOW);
}
void checkPassword() {
  if (strcmp(enteredPassword, password) == 0) {
    lcd.clear();
    lcd.print("Clave Correcta");
    delay(1000);  // Small delay to visualize the message
    input = Forward;
    attempts=0;
    /*
    // Measure humidity, temperature, and light intensity
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int lightIntensity = analogRead(LDR_PIN);
    

    if (!isnan(h) && !isnan(t)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Humedad: ");
      lcd.print(h);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("C:");
      lcd.print(t);
      lcd.print("");
      lcd.setCursor(9, 1);
      lcd.print("L:");
      lcd.print(lightIntensity);
      // Check if temperature is greater than 32°C and light is less than 40
      if (t > 32 ) {
        lcd.clear();
        lcd.print("Sistema Bloqueado");
        digitalWrite(redLED, HIGH);
        delay(4000);  // Keep red LED on for 4 seconds
        // Blink red LED for 800ms every 500ms for 3 seconds
        for (int i = 0; i < 6; i++) {
          digitalWrite(redLED, HIGH);
          delay(800);
          digitalWrite(redLED, LOW);
          delay(200);
        }
        resetSystem();  // Call the function to reset the system
      
      }
    } else {
      lcd.clear();
      lcd.print("Error en sensor");
    }*/
    digitalWrite(greenLED, HIGH);
    delay(2000);
    digitalWrite(greenLED, LOW);
  } else {
    attempts++;
    if (attempts >= 3) {
      lcd.clear();
      lcd.print("Sistema Bloqueado");
      digitalWrite(redLED, HIGH);
      delay(5000);
      resetSystem();  // Call the function to reset the system
      // Add code to implement reset mechanism here
    } else {
      lcd.clear();
      lcd.print("Clave Incorrecta");
      digitalWrite(blueLED, HIGH);
      delay(1000);
      digitalWrite(blueLED, LOW);
    }
  }
}