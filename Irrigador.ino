#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

#define pinTemp A1
#define pinReleBomba1 3
#define pinReleBomba2 7
#define pinBotaoBomba 5
#define pinAcionamentoTempo 2

/* ------------------------------------------------------------------------------------------------------------------------
 *  Autor...: Heriberto Giannasi
 *  Data....: 01/05/2018 
 *  Objetivo: Controlar irrigador com acionamento sequencial de duas bombas RS-385 para gotejamento.
 *  O aparelho possui modo de irrigação automatico que define a quantidade de irrigações diarias a partir da temperatura.
 *  O aparelho possui modo de irrigação manual onde é definido manualmente o numero de irrigações diarias.
 * ------------------------------------------------------------------------------------------------------------------------
 * Componentes:
 * 
 * Nome          Pino               Função
 * ------------- ------------------ ------------------------------------------
 * LM35          Analógico 2        Medir a temperatura
 * Push boton    Digital 2          Definir modo de irrigação (Manual ou Auto) 
 * Push boton    Digital 5          Acionar bomba Rs385 manualmente
 * Rele Canal 1  Digital 3          Controla acionamento da bomba Rs385-1
 * Rele Canal 2  Digital 7          Controla acionamento da bomba Rs385-2
 * Display 16x2  Digital 8 a 13     Exibir informações de controle
 * ----------------------------------------------------------------------------------------------------------------------*/

LiquidCrystal_I2C lcd(0x3F,2,1,0,4,5,6,7,3, POSITIVE);

/*
 * Constantes de parametro do sistema
 */
  const int INTERVALO_MAXIMO = 1440;
  const int TEMPO_ACIONAMENTO = 300;
  const int INCREMENTO_INTERVALO = 360;
  const int IRRIGADOR_AUTOMATICO = 0;
  const int LENGTH_HIST_TEMP     = 30;
  const int DELAY_PADRAO = 985;
 
/*
 * Variaveis globais para tratamento da temperatura
 */
  int temperatura = 0;
  int histTemp [100];
  boolean erroMedicao = false;
  
/*
 * Variaveis para tratamento do tempo decorrido
 */
  unsigned long contadorSegundos = 0;
  unsigned long contadorMinutos = 0;
  unsigned long contadorMinutosAcionamento = 0;
  
/*
 * Variaveis para tratamento do acionamento das bombas
 */
  unsigned int parametroIntervalo = 0;
  int intervalo = 0;
  bool acionarBomba;

/*
 * Variaveis para utilização no display
 */
  boolean exibeDoisPontos = true;
  byte statusBar1[8] = {B11111, B00000, B11111, B11111, B11111, B11111, B00000, B11111};
  byte statusBarBorda1[8] = {B11111, B00000, B00000, B00000, B00000, B00000, B00000, B11111};
  
/*
 *  Endereço EEPROM para persistir o contador de minutos para o 
 *  caso de desligamento
 */

const int ADDRESS_MIN_L = 500;
const int ADDRESS_MIN_H = 501;
const int ADRRESS_ESTADO_BOMBA1 = 401;
const int ADRRESS_ESTADO_BOMBA2 = 402;
const int ADRRESS_PARAM_L = 300;
const int ADDRESS_PARAM_H = 301;

void setup() {
    Serial.begin(9600);

    iniciarLCD();
    iniciarEdefinirPinos();
    obterContadorMinutos();
    valoresIniciaisHistorico();
    obterParametroIntervalo();

    testarParametros();    
}

void loop() {
    atualizarParmDisplay();

    acionarBomba = false;
    tempoEspera();  
    testarParametros();    
    if (acionarBomba)ligarBomba();      
} 

/*
 * funcoes de inicialização
 */
void iniciarLCD(){
    lcd.begin(16,2);
    lcd.createChar(1, statusBar1);
    lcd.createChar(4, statusBarBorda1);
}

void iniciarEdefinirPinos(){
    pinMode(pinBotaoBomba, INPUT_PULLUP);
    pinMode(pinAcionamentoTempo, INPUT_PULLUP);
    pinMode(pinReleBomba1, OUTPUT);
    pinMode(pinReleBomba2, OUTPUT);
    
    digitalWrite(pinReleBomba1, HIGH);     
    digitalWrite(pinReleBomba2, HIGH);     
}

void valoresIniciaisHistorico(){
    for(int i=0; i < LENGTH_HIST_TEMP; i++) histTemp[i] = 0;
}

void obterContadorMinutos(){
    int estadoBomba1 = EEPROM.read(ADRRESS_ESTADO_BOMBA1);
    if (estadoBomba1 == 1){
      EEPROM.write(ADRRESS_ESTADO_BOMBA1,0);
      contadorMinutos = contadorMinutosAcionamento = 0;
      return;
    }

    int estadoBomba2 = EEPROM.read(ADRRESS_ESTADO_BOMBA2);
    if (estadoBomba2 == 1){
      EEPROM.write(ADRRESS_ESTADO_BOMBA2,0);
      contadorMinutos = contadorMinutosAcionamento = 0;
      return;
    }
        
    byte low = EEPROM.read(ADDRESS_MIN_L);
    byte high = EEPROM.read(ADDRESS_MIN_H);
    int value = word(high,low);

    if (value < 0) 
        value = 0;
    
    contadorMinutos = contadorMinutosAcionamento = value;
}

void obterParametroIntervalo(){
    byte low = EEPROM.read(ADRRESS_PARAM_L);
    byte high = EEPROM.read(ADDRESS_PARAM_H);
    int value = word(high,low);

    if (value < 0) 
        value = 0;
    
    parametroIntervalo = value;
}

/*
 * Funcoes de processamento
 */
 
void testarParametros(){
    temperatura = obterTemperatura();
    testarAcionamentoDiario();
    testarAcionamentoManual();
    testarAdicaoTempoAcionamento();
}

void testarAcionamentoDiario(){
/*
 * Função responsável por solicitar o acionamento das bombas 
 * após o intervalo decorrido
 */
    calculoIntervalo();
    if (contadorMinutos >= intervalo) acionarBomba = true;
}

void calculoIntervalo(){
/*
 * Funcao responsavel por calcular o intervalo em minutos
 * parametrizado para o acionamento da bomba.
 * O parametro esteja com 0 (zero), identifica o modo de irrigação
 * automatico.
 */
    if (parametroIntervalo > 0){
        intervalo = parametroIntervalo;
    }
    else {
    //Definicao da quantidade de acionamento de acordo com a temperatura
      int ref = 0;
      
      if      (temperatura <= 12)                      ref = 1;
      else if (temperatura >  12 && temperatura <= 17) ref = 2;
      else if (temperatura >  17 && temperatura <= 27) ref = 3;
      else if (temperatura >  27)                      ref = 4;
      
      intervalo = INTERVALO_MAXIMO/ref;
    }
}

int obterTemperatura(){  
  /*
   * Função responsável por obter a temperatura do ambiente
   * a temperatura será definida a partir da média das ultimas 10
   * temperaturas medidas.
   */
    carregarHistoricoTemperatura();
    return calcularMediaTemperatura();  
}

void carregarHistoricoTemperatura(){
  //Função para definir a temperatura a partir da leitura analiógica
    
    int temp = int(((analogRead(pinTemp)*5.0)/1023.0)/0.01);

  // Consistencia para desprezar erros de leitura
    erroMedicao = false;

    if (temp < 5 || temp > 40){
        temp = 21;
        erroMedicao = true;
    }

    int diff = 0;
    if (histTemp[LENGTH_HIST_TEMP] != 0){
      if (histTemp[LENGTH_HIST_TEMP] > temp)
          diff = histTemp[LENGTH_HIST_TEMP] - temp;
      else 
          diff = temp - histTemp[LENGTH_HIST_TEMP];
    }

    if (diff > 2){
        temp = 21;
        erroMedicao = true;
    }
      
    
  //Historico para calculo de media com 10 medições
    for(int i=1; i < (LENGTH_HIST_TEMP - 1); i++) histTemp[i] = histTemp[i+1];
   
    histTemp[LENGTH_HIST_TEMP - 1] = temp;
}

int calcularMediaTemperatura(){
  //Calculo da media historica
    int index = 0;
    int soma = 0;
    for (int i=0; i < LENGTH_HIST_TEMP; i++){
      if (histTemp[i] != 0){
          index++;
          soma = soma + histTemp[i];
      }
    }  
    return soma/index;
}

void testarAcionamentoManual(){
    if (!digitalRead(pinBotaoBomba)) acionarBomba = true;
}

void testarAdicaoTempoAcionamento(){
    if (!digitalRead(pinAcionamentoTempo)) {
        adicionarAcionamentoTempo();
        lcd.clear();
    }
}

void adicionarAcionamentoTempo(){
  /*
   * Adiciona 30 min para o intervalo de acionamento da bomba. 
   * Quando parametro estiver com 0 (zero) o irrigador trabalha no modo
   * automático
   */
    parametroIntervalo = parametroIntervalo + INCREMENTO_INTERVALO;
  
    if (parametroIntervalo > INTERVALO_MAXIMO)
        parametroIntervalo = IRRIGADOR_AUTOMATICO;    

    lcd.clear();  
    lcd.print("Intervalo acion.");                      
               
    if (parametroIntervalo == IRRIGADOR_AUTOMATICO){
        lcd.setCursor(0,1);
        lcd.print("AUTO");
    }
    else{
        lcd.setCursor(0,1);
        if (calcularHora(parametroIntervalo) < 10) lcd.print("0");
        lcd.print(calcularHora(parametroIntervalo));
        lcd.print(":");

        if(calcularMinuto(parametroIntervalo) < 10) lcd.print("0");
        lcd.print(calcularMinuto(parametroIntervalo));
    }
    persistirParametro();
    
    delay(700);
    testarAdicaoTempoAcionamento();
}

void persistirParametro(){
   byte high = highByte(parametroIntervalo);
   byte low  = lowByte(parametroIntervalo);

   EEPROM.write(ADRRESS_PARAM_L, low);
   EEPROM.write(ADDRESS_PARAM_H, high);
}

void atualizarParmDisplay(){  
    // LINHA 01
    lcd.setCursor(0,0);
    lcd.print("Int:");

    if (calcularHora(intervalo) < 10) lcd.print("0");
    lcd.print(calcularHora(intervalo));
    
    if (exibeDoisPontos) lcd.print(":");
    else lcd.print(" ");

    if (calcularMinuto(intervalo) < 10) lcd.print("0");
    lcd.print(calcularMinuto(intervalo));

    lcd.print("-");
    lcd.print(INTERVALO_MAXIMO/intervalo);
    lcd.print(" ");

    if (parametroIntervalo == IRRIGADOR_AUTOMATICO){
      lcd.setCursor(13,0);
      lcd.print("AUT");
    }

    // LINHA 02
    lcd.setCursor(0,1);
    lcd.print("Ult:");

    if (calcularHora(contadorMinutos) < 10) lcd.print("0");
    lcd.print(calcularHora(contadorMinutos));

    if (exibeDoisPontos) lcd.print(":");
    else lcd.print(" ");

    if (calcularMinuto(contadorMinutos) < 10) lcd.print("0");
    lcd.print(calcularMinuto(contadorMinutos));

    lcd.setCursor(13,1);
    int temp = temperatura;
    if (temp < 10) lcd.print("0");
    lcd.print(temp);
    
    if (erroMedicao) lcd.print("E");
    else lcd.print("C");

    exibeDoisPontos = !exibeDoisPontos;
}

int calcularHora(int hora){
    return  hora / 60;
}

int calcularMinuto(int minuto){
    return minuto % 60;
}

void ligarBomba(){
   /*
   if (contadorMinutosAcionamento < INCREMENTO_INTERVALO){
       lcd.clear();
       lcd.setCursor(0,0);
       lcd.print("AGUARDAR INTERV.");
       lcd.setCursor(0, 1);
       lcd.print("MINIMO");
       delay(2000);
       lcd.clear();
       return;
   }*/
  
   boolean ponto1=true, ponto2=true, ponto3=true;
   
   lcd.clear();
   digitalWrite(pinReleBomba1, LOW);
   EEPROM.write(ADRRESS_ESTADO_BOMBA1, 1);     
   
   lcd.setCursor(0,0);
   lcd.print("IRRIGACAO ATIVA...");
   
   statusAcionamento();
   desligarBomba1();

   contadorMinutos = contadorMinutosAcionamento =0;
   persistirMinuto();
   lcd.clear();
}

void statusAcionamento(){
   for(int i=0; i < 16; i++){
      lcd.setCursor(i,1);
      lcd.write(4);
   }
   
   int resto = 0, colStatus = 0;
   float percentual = 0;

   unsigned int tempoAcionamento = TEMPO_ACIONAMENTO;
   
   for(int contadorAcionamento = 0; contadorAcionamento < tempoAcionamento; contadorAcionamento++){
      if (contadorAcionamento  > 0) resto = contadorAcionamento % 3;
        
      lcd.setCursor(13,0);
      lcd.print("."); 
      
      lcd.setCursor(14,0);
      if(resto > 0) lcd.print("."); 
      else lcd.print(" "); 
        
      lcd.setCursor(15,0);
      if (resto > 1) lcd.print("."); 
      else lcd.print(" ");  

      percentual = float(contadorAcionamento)/float(tempoAcionamento);
      colStatus = int(percentual / 0.0625);
      
      lcd.setCursor(colStatus,1);
      lcd.write(1);

      delay(1000);
   }
}

void desligarBomba1(){
    digitalWrite(pinReleBomba1, HIGH);
    EEPROM.write(ADRRESS_ESTADO_BOMBA1, 0);     
}

void desligarBomba2(){
    digitalWrite(pinReleBomba2, HIGH);
    EEPROM.write(ADRRESS_ESTADO_BOMBA2, 0);     
}

void tempoEspera(){
    delay(DELAY_PADRAO);
    contadorSegundos++;

    if (contadorSegundos >= 60){
        contadorMinutos++;
        contadorMinutosAcionamento = contadorMinutos;
        contadorSegundos = 0;
        persistirMinuto();
    }          
}

void persistirMinuto(){
   byte high = highByte(contadorMinutos);
   byte low  = lowByte(contadorMinutos);

   EEPROM.write(ADDRESS_MIN_L, low);
   EEPROM.write(ADDRESS_MIN_H, high);
}
