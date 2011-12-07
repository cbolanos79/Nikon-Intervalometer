/*
Copyright (c) 2011, Cristo Saulo Bolaños Trujillo - cbolanos@gmail.com
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of copyright holders nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.*/

/*
  Versión 0.1: Primera versión lista para probar a nivel de software sólo con intervalómetro de nocturnas.
               Pendiente implementación de hardware.
               Usa el LCD Keypad Shield como primer soporte
  
*/
#include <LiquidCrystal.h>
#include <MsTimer2.h>
#include <nikonIrControl.h>

#define LCDKEYPAD_SHIELD
#define DEBUG

#define STATUS_VIEW 0
#define STATUS_SELECT 1
#define STATUS_EDIT 2

#define MENU_DATA 0
#define MENU_COUNTDOWN 1

#define SET_BUTTON 0
#define CANCEL_BUTTON 1
#define PLUS_BUTTON 2
#define MINUS_BUTTON 3

// Botones
#define SET_BUTTON_PIN 0
#define CANCEL_BUTTON_PIN 1
#define PLUS_BUTTON_PIN 2
#define MINUS_BUTTON_PIN 3

// Posiciones en el menú de datos
#define MENU_DATA_INPUT_ISO 0
#define MENU_DATA_INPUT_F 1
#define MENU_DATA_INPUT_TIME 2
#define MENU_DATA_OUTPUT_ISO 3
#define MENU_DATA_OUTPUT_F 4
#define MENU_DATA__LIMIT 5

// Estado actual: editar ajustes, cuenta atrás
volatile int current_status;

// Menú actual: datos, cuenta atrás
volatile int current_menu;

// Posición en el menú de datos
volatile int menu_data_pos;

// Botón pulsado
volatile int pressed_button;

int menu_data_pos_table[6][2] = { // Fila 0: entrada
                            {1,0}, {7, 0}, {11, 0},
                            // Fila 1: salida
                            {1,1}, {7, 1}, {-1, -1}
                          };
                          
// Valores ISO
const char iso_values[][5] = {" 200", " 400", " 800", "1600"};
const int iso_values_count = 4;
volatile int iso_input_pos, iso_output_pos;

// Valores de diafragma
const char f_values[][5] = {"  1", "1.4", "  2", "2.8", "  4", "5.6", "  8", " 11", " 16", " 22"};
const int f_values_count = 10;
volatile int f_values_pos;
volatile int f_input_pos, f_output_pos;

// Valores de exposición (propios de la cámara Nikon D40)
const float t_values[] = {1, 1.3, 1.6, 2, 2.5, 3, 4, 5, 6, 8, 10, 13, 15, 20, 25, 30};
const float t_values_count = 16;
volatile int t_input_pos;
volatile float t_output_value;

#ifdef LCDKEYPAD_SHIELD
  LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#endif

#ifdef RED_LCD
  LiquidCrystal lcd(3, 5, 7, 8, 10, 12);
#endif

// Pin del led infrarrojo que dispara la cámara
int ir_pin = 2;

void imprime_error() {
    lcd.setCursor(0, 0);
    lcd.print("ERROR");
    delay(2000);
    
    lcd.setCursor(0, 0);
    lcd.print("I");
    lcd.print(iso_values[iso_input_pos]);
  
}

void data_view_set() {
  #ifdef DEBUG
    Serial.println(F("data view set"));
  #endif
  current_status = STATUS_SELECT;
  lcd.setCursor(menu_data_pos_table[menu_data_pos][0], menu_data_pos_table[menu_data_pos][1]);
  lcd.blink();
}

// Calcula el tiempo de exposición a partir de los parámetros recibidos
void data_view_cancel() {
  // No tiene sentido usar parámetros de salida menores que los de entrada
  if ((iso_output_pos > iso_input_pos) || (f_output_pos < f_input_pos)) {
    imprime_error();    
  } else {
    // En caso contrario, calcula la exposición correcta
    
    // El número de pasos es la diferencia de pasos de ISO + la diferencia de pasos de diafragma
    int stops = (iso_input_pos - iso_output_pos) + (f_output_pos - f_input_pos);
    
    // El tiempo total es el tiempo de exposición medido multiplicado por 2^número pasos
    t_output_value = t_values[t_input_pos] * pow(2, stops);
    
    lcd.setCursor(11, 1);
    lcd.print(int(t_output_value));
    lcd.print("s");
  }
}

void data_view_plus() {
  // No activa la cuenta atrás si no se ha calculado primero el tiempo
  if (t_output_value == -1) {
    imprime_error();    
    return;
  }
  #ifdef DEBUG
    Serial.println(F("data view plus"));
  #endif
  lcd.clear();  
  lcd.print("Cuenta atrás");

  // Activa el contador para la cuenta atrás
  MsTimer2::start();
  
  current_menu = MENU_COUNTDOWN;
  
  // Dispara para empezar la exposición
  cameraSnap(ir_pin);
}

void data_select_set() {
  #ifdef DEBUG
    Serial.println(F("data select set"));
  #endif
  current_status = STATUS_EDIT;
}

void data_select_cancel() {
  #ifdef DEBUG
    Serial.println(F("data select cancel"));
  #endif
  current_status = STATUS_VIEW;
  lcd.noBlink();
}

void data_select_plus() {
  #ifdef DEBUG
    Serial.println(F("data select plus"));
  #endif
 
  menu_data_pos++;
  if (menu_data_pos >= MENU_DATA__LIMIT)
    menu_data_pos = 0;

  lcd.setCursor(menu_data_pos_table[menu_data_pos][0], menu_data_pos_table[menu_data_pos][1]);
}

void data_select_minus() {
  #ifdef DEBUG
    Serial.println(F("data select minus"));
  #endif
  menu_data_pos--;
  if (menu_data_pos < 0)
    menu_data_pos = MENU_DATA__LIMIT - 1;

  lcd.setCursor(menu_data_pos_table[menu_data_pos][0], menu_data_pos_table[menu_data_pos][1]);
}

void data_edit_set() {
  #ifdef DEBUG
    Serial.println(F("data edit set"));
  #endif
  current_status = STATUS_SELECT;
}

void data_edit_cancel() {
  #ifdef DEBUG
    Serial.println(F("data edit cancel"));
  #endif
  current_status = STATUS_SELECT;
}

void data_edit_plus() {
  #ifdef DEBUG
    Serial.println(F("data edit plus"));
  #endif
  
  lcd.setCursor(menu_data_pos_table[menu_data_pos][0], menu_data_pos_table[menu_data_pos][1]);
  switch(menu_data_pos) {
    case MENU_DATA_INPUT_ISO:
      iso_input_pos++;
      if (iso_input_pos >= iso_values_count)
        iso_input_pos = 0;
        
      lcd.print(iso_values[iso_input_pos]);
    break;
    
    case MENU_DATA_INPUT_F:
      f_input_pos++;
      if (f_input_pos >= f_values_count)
        f_input_pos = 0;
        
      lcd.print(f_values[f_input_pos]);
    break;
    
    case MENU_DATA_INPUT_TIME:
      t_input_pos++;
      if (t_input_pos >= t_values_count)
        t_input_pos = 0;
        
      lcd.print(int(t_values[t_input_pos]));
      lcd.print("s");
    break;
    
    case MENU_DATA_OUTPUT_ISO:
      iso_output_pos++;
      if (iso_output_pos >= iso_values_count)
        iso_output_pos = 0;
        
      lcd.print(iso_values[iso_output_pos]);
    break;
    
    case MENU_DATA_OUTPUT_F:
      f_output_pos++;
      if (f_output_pos >= f_values_count)
        f_output_pos = 0;
        
      lcd.print(f_values[f_output_pos]);
    break;
     
  }
}

void data_edit_minus() {
  #ifdef DEBUG
    Serial.println(F("data edit minus"));
  #endif

  lcd.setCursor(menu_data_pos_table[menu_data_pos][0], menu_data_pos_table[menu_data_pos][1]);
  switch(menu_data_pos) {
    case MENU_DATA_INPUT_ISO:
      iso_input_pos--;
      if (iso_input_pos < 0)
        iso_input_pos = iso_values_count - 1;
        
      lcd.print(iso_values[iso_input_pos]);
    break;
    
    case MENU_DATA_INPUT_F:
      f_input_pos--;
      if (f_input_pos < 0)
        f_input_pos = f_values_count - 1;
        
      lcd.print(f_values[f_input_pos]);
    break;
    
    case MENU_DATA_INPUT_TIME:
      t_input_pos--;
      if (t_input_pos < 0)
        t_input_pos = t_values_count - 1;
        
      lcd.print(int(t_values[t_input_pos]));
      lcd.print("s");
      
    break;
    
    case MENU_DATA_OUTPUT_ISO:
      iso_output_pos--;
      if (iso_output_pos < 0)
        iso_output_pos = iso_values_count - 1;
        
      lcd.print(iso_values[iso_output_pos]);
    break;
    
    case MENU_DATA_OUTPUT_F:
      f_output_pos--;
      if (f_output_pos < 0)
        f_output_pos = f_values_count - 1;
        
      lcd.print(f_values[f_output_pos]);
    break;
     
  }
}

void countdown_view_cancel() {
  #ifdef DEBUG
    Serial.println(F("countdown view cancel"));
  #endif
  MsTimer2::stop();
  current_status = STATUS_VIEW;
  current_menu = MENU_DATA;
  init_values();
  print_init_menu();
  
  // Finaliza la exposición
  cameraSnap(ir_pin);
}

typedef void (*fptr)();

/*
  Tabla de acciones según el menú, estado y acción  
*/

fptr action_table[2][3][4] = {
                            // Menú datos
                            {                              
                              // Ver
                              {data_view_set, data_view_cancel, data_view_plus, NULL},
                              
                              // Seleccionar
                              {data_select_set, data_select_cancel, data_select_plus, data_select_minus},
                            
                              // Editar
                              {data_edit_set, data_edit_cancel, data_edit_plus, data_edit_minus},
                            },
                            
                            // Menú cuenta atrás
                            {
                              // Ver
                              {NULL, countdown_view_cancel, NULL, NULL},
                            
                              // Seleccionar
                              {NULL, NULL, NULL, NULL},
                              
                              // Editar
                              {NULL, NULL, NULL, NULL},
                             },
                           };

// Cuenta atrás para disparar el obturador
void countdown() {
  // Cuando la cuenta atrás llega a 0, dispara el obturador y vuelve al principio
  if (t_output_value  <= 0) {
    // Finaliza la exposición
    cameraSnap(ir_pin);
  
    MsTimer2::stop();
    current_status = STATUS_VIEW;
    current_menu = MENU_DATA;
    init_values();
    print_init_menu();
  } else {
    lcd.setCursor(0, 0);
    lcd.print(int(t_output_value));
    lcd.print("s");
    lcd.print(F("         "));
  }
  t_output_value--;
}

void print_init_menu() {
  lcd.setCursor(0, 0);
  lcd.print("I");
  lcd.print(iso_values[iso_input_pos]);
  lcd.print(" f");
  lcd.print(f_values[f_input_pos]);
  lcd.print(" ");
  lcd.print(int(t_values[t_input_pos]));
  lcd.print("s");
  
  lcd.setCursor(0, 1);
  lcd.print("I");
  lcd.print(iso_values[iso_output_pos]);
  lcd.print(" f");
  lcd.print(f_values[f_output_pos]);
  
}

void init_values() {
  pressed_button = -1;
  current_status = STATUS_VIEW;
  current_menu = MENU_DATA;
  /*
  menu_data_pos = 0;
  iso_input_pos = 0;
  iso_output_pos = 0;
  
  f_input_pos = 0;
  f_output_pos = 0;

  t_input_pos = 0;  
  */
  t_output_value = -1;
}

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
  #endif
  lcd.begin(16, 2);
  
  init_values();
  
  print_init_menu();
  
  // Contador para la cuenta atrás en el disparo
  MsTimer2::set(1000, countdown);

  t_output_value = -1;
  
  pinMode(ir_pin, OUTPUT);

}

void loop() {
  pressed_button = -1;

  #ifdef LCDKEYPAD_SHIELD
    int button = analogRead(0);
    #ifdef DEBUG
      //Serial.println(button);
    #endif
    
    // Registra el botón pulsado
    if ((button>700) && (button<800)) {
      pressed_button = SET_BUTTON;
    } else if ((button>400) && (button<500)) {
        pressed_button = CANCEL_BUTTON;
    } else if ((button>100) && (button<200)) {
        pressed_button = PLUS_BUTTON;
    } else if ((button>300) && (button<400)) {
        pressed_button = MINUS_BUTTON;
    }
    
    #ifdef DEBUG
      if (pressed_button > -1) {
        Serial.println(pressed_button);
        delay(400);
      }
    #endif
    
  #endif
  
  // Según el estado, y el botón pulsado, ejecuta una acción
  if (pressed_button > -1) {
    Serial.print(current_menu);
    Serial.print(" ");
    Serial.print(current_status);
    Serial.print(" ");
    Serial.print(pressed_button);
    Serial.println();
    action_table[current_menu][current_status][pressed_button]();
  }
}

//SELECT: 718-719
//LEFT: 477-478
//UP: 128-129
//DOWN: 304-305
