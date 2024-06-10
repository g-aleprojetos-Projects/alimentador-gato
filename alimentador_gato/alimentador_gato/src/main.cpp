#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS1307.h>

#define LEDAzul 2
#define out 16
#define micro_switch 14
#define rele 12

DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =================================================================================================
// --- Protótipo das Funções ---
String getDataHora();
String getTempoRestante();
void verificarEAcionarAlarme();
void atualizarLCD();

// =================================================================================================
// --- Variáveis Globais ---
char counter = 0x00; // variável auxiliar de contagem
int alarmes[5][2] = {{7, 0}, {11, 0}, {15, 0}, {19, 0}, {23, 0}};
int proximoAlarme = 0;
volatile bool atualizar = false;
int state_micro_switch = 0;
int previous_state_micro_switch = 0;

// =================================================================================================
// --- Rotina de Interrupção ---
void IRAM_ATTR onTimerISR()
{
  timer1_write(100000);
  counter++; // incrementa counter

  if (counter == 0x05)                    // counter
  {                                       // sim...
    digitalWrite(out, !digitalRead(out)); // Inverte o estado da saída de teste
    state_micro_switch = digitalRead(micro_switch);
    counter = 0x00;
    atualizar = true;
  } // end if counter
}

// =================================================================================================
// -- Configurações iniciais --
void setup()
{
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  pinMode(micro_switch, INPUT);
  pinMode(rele, OUTPUT);
  pinMode(out, OUTPUT);
  pinMode(LEDAzul, OUTPUT);
  digitalWrite(rele, LOW);
  digitalWrite(LEDAzul, HIGH);
  // Initialize Ticker every
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(100000); // 1 segundo

  lcd.setCursor(0, 0);
  lcd.print("ALIMENTADOR DE GATO");
  lcd.setCursor(3, 2);
  lcd.print("Init RTC...");
  Serial.println("Init RTC...");
  rtc.begin();
  rtc.set(50, 59, 14, 7, 6, 2024);
  rtc.start();
}

// =================================================================================================
// -- Loop infinito --
void loop()
{
  if (atualizar)
  {
    atualizarLCD();
    atualizar = false;
  }

  if(state_micro_switch == HIGH)
  {
    delay(150);
    digitalWrite(LEDAzul, HIGH);
  }

  // Verifica se houve uma transição de HIGH para LOW
  if (previous_state_micro_switch == HIGH && state_micro_switch == LOW)
  {
    // Se houve uma transição de HIGH para LOW, desliga o relé
    delay(150);
    digitalWrite(LEDAzul, LOW);
    digitalWrite(rele, LOW);
  }

  if (previous_state_micro_switch != state_micro_switch)
  {
    // Atualiza o estado anterior do micro_switch
    previous_state_micro_switch = state_micro_switch;
  }
}

// =================================================================================================
// -- Funções --

// Função para obter a data e hora do RTC
String getDataHora()
{
  uint8_t sec, min, hour, day, month;
  uint16_t year;

  // get time from RTC
  rtc.get(&sec, &min, &hour, &day, &month, &year);

  String data = "";
  if (day < 10)
    data += "0";
  data += String(day, DEC);
  data += "/";
  if (month < 10)
    data += "0";
  data += String(month, DEC);
  data += "/";
  data += String(year, DEC);

  String hora = "";
  if (hour < 10)
    hora += "0";
  hora += String(hour, DEC);
  hora += ":";
  if (min < 10)
    hora += "0";
  hora += String(min, DEC);
  hora += ":";
  if (sec < 10)
    hora += "0";
  hora += String(sec, DEC);
  return data + " " + hora;
}

// Função para obter o tempo restante até o próximo alarme
String getTempoRestante()
{
  uint8_t sec, min, hour, day, month;
  uint16_t year;
  rtc.get(&sec, &min, &hour, &day, &month, &year);

  // Calcula os minutos atuais desde a meia-noite
  int minutosAgora = hour * 60 + min;

  verificarEAcionarAlarme();

  // Encontra o próximo alarme
  proximoAlarme = 0;
  int minutosProximoAlarme = alarmes[0][0] * 60 + alarmes[0][1];
  for (int i = 1; i < 5; i++)
  {
    int minutosAlarme = alarmes[i][0] * 60 + alarmes[i][1];
    if (minutosAlarme > minutosAgora && (minutosAlarme < minutosProximoAlarme || minutosProximoAlarme <= minutosAgora))
    {
      proximoAlarme = i;
      minutosProximoAlarme = minutosAlarme;
    }
  }

  // Calcula o tempo restante até o próximo alarme
  int minutosRestantes = minutosProximoAlarme - minutosAgora;
  if (minutosRestantes <= 0)
    minutosRestantes += 24 * 60;

  // Subtrai um minuto se os segundos forem diferentes de zero
  if (sec != 0)
  {
    minutosRestantes--;
    sec = 60 - sec;
  }

  // Formata o tempo restante como uma string
  String tempoRestante = "";
  if (minutosRestantes / 60 < 10)
    tempoRestante += "0";
  tempoRestante += String(minutosRestantes / 60) + ":";
  if ((minutosRestantes % 60) < 10)
    tempoRestante += "0";
  tempoRestante += String(minutosRestantes % 60) + ":";
  if (sec < 10)
    tempoRestante += "0";
  tempoRestante += String(sec);

  return tempoRestante;
}

// Função para atualizar o LCD
void atualizarLCD()
{
  String data = getDataHora();
  lcd.setCursor(0, 1);
  lcd.print(data);
  lcd.setCursor(1, 2);
  lcd.print("Proxima Refeicao:");
  String tempoRestante = getTempoRestante();
  lcd.setCursor(5, 3);
  lcd.print(tempoRestante);
}

// Função para verificar e acionar o alarme
void verificarEAcionarAlarme()
{
  uint8_t sec, min, hour, day, month;
  uint16_t year;
  rtc.get(&sec, &min, &hour, &day, &month, &year);
  // Verifica se é hora do próximo alarme
  if (hour == alarmes[proximoAlarme][0] && min == alarmes[proximoAlarme][1] && sec == 0)
  {
    digitalWrite(rele, HIGH);

    // Atualiza o próximo alarme
    proximoAlarme = (proximoAlarme + 1) % 5;
  }
}
