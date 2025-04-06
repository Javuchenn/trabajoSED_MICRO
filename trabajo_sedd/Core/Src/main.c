/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/*/////////////////////////////////////////////////////////////////////////////////
***********************************************************************************
*************************************MP3+OLED**************************************
***********************************************************************************
En este proyecto han sido utilizados varios drivers para el control del audio del micro y del OLED que ha sido adquirido.
Estos archivos han sido descargados de la pagina de Controllerstech
  - Los drivers para gestionar audio son: AUDIO_LINK, AUDIO, cs43l22, File_Handling y waveplayer
  - Los drivers para gestionar el OLED son: fonts y ssd1306

**** DRIVERS MODIFICADOS POR EL ALUMNO: waveplayer.c
  - Se ha modificado waveplayer.c para incluir dos funciones que permiten la
    obtencion del nombre del archivo de audio que se esta reproduciendo: get_name() y quitar_extension()

  - Tambien se ha modificado la funcion AUDIO_ErrorTypeDef AUDIO_PLAYER_Process() para actualizar el volumen cuando sea necesario

**** RESTO DE ARCHIVOS (REALIZADOS AL 100% POR EL ALUMNO)
  - Se han creado archivos de cabecera (.h) para realizar mapas de bits con dibujos/animaciones
    en el OLED, estos archivos son: bailadora.h, fumador.h, skater.h y Otras_Imagenes.h

  - main.c: En este archivo, el funcionamiento principal del programa es gestionado

**** FUNCIONAMIENTO PRINCIPAL
  - Al ejecutar el programa se muestra en el OLED una portada durante 5 segundos
  - Despues se muestra una segunda portada durante otros 5 segundos
  - Al terminar la presentacion, empieza a sonar la musica y en el OLED se muestra el nombre de la cancion acompanado de una animacion
  - Las animaciones son gestionadas con un temporizador basico
  - Con PA0, PA1 y PA2 se controla la accion de pausar y pasar a siguiente o anterior cancion
  - Al cambiar de cancion, se actualiza en el OLED la animacion mostrada y el titulo de cancion
  - Al pausar se muestra una imagen especial
  - Potenciometro y Convertidor AD:
    - El volumen de la musica se controla con el potenciometro
    - El potenciometro tiene tres terminales, uno se conecta 5V, otro a GND y el del medio a PA3
    - La tension que llega a PA3 es convertida a valor digital gracias al ADC
    - Una vez tenemos este valor digital podemos controlar el volumen en el codigo de forma comoda
*/

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

//////AUDIO///////
#include "waveplayer.h"
#include "File_Handling.h"

//////OLED///////
#include "ssd1306.h"
#include "fonts.h"
#include "stdio.h"

//////ANIMACIONES e IMAGENES///////
#include "skater.h"
#include "fumador.h"
#include "bailadora.h"
#include "Otras_Imagenes.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

I2S_HandleTypeDef hi2s3;
DMA_HandleTypeDef hdma_spi3_tx;

TIM_HandleTypeDef htim10;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM10_Init(void);
static void MX_ADC1_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//////////////////////////////ADC///////////////
uint32_t ADC_val;     // Valor digital
uint32_t volumen;     // Valor del volumen convertido al rango 1-100
char buf_volumen[4];  // Cadena de caracteres que almacena el valor del volumen para representarlo en el OLED
////////////////////////////////////////////////

/////////////////ADC/////////////////////////////////////////////////////
uint32_t map_to_scale(uint32_t input)
{
    // Mapea de 0–255 a 1–100
    return (uint32_t)(((input * 99) / 255) + 1);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1)
	{
		ADC_val = HAL_ADC_GetValue(hadc);      // Obtener el valor convertido
		volumen = map_to_scale(ADC_val);       // Convertir el valor al rango 1-100
		sprintf (buf_volumen, "%d", volumen);  // Almacenar el valor del volumen en cadena de caracteres
	}
}
///////////////////////////////////////////////////////////////////////////

// Declaracion de las funciones que controlan las animaciones
void animacion1(uint8_t);
void animacion2(uint8_t);
void animacion3(uint8_t);
void animacion4(uint8_t);

extern ApplicationTypeDef Appli_state;         // Estado de la aplicacion
extern AUDIO_PLAYBACK_StateTypeDef AudioState; // Estado del audio (Siguiente o anterior cancion, pausa...)

int IsFinished = 0;               // Para terminar el bucle while principal cuando AudioState = AUDIO_STATE_STOP
int current_anim_state = 3;       // Animacion que se esta ejecutando en el momento (1, 2, 3 o 4)
volatile uint8_t frame_anim = 0;  // Variable para selecionar los frames en las animaciones

void change_animation() // Para seleccionar la siguiente animacion al cambiar de cancion
{
	// Se hace con esta estructura if-elsif porque con los rebotes "current_anim_state ++" daba errores
	if(current_anim_state == 1)
		current_anim_state = 2;
	else if(current_anim_state == 2)
		current_anim_state = 3;
	else if(current_anim_state == 3)
		current_anim_state = 4;
	else if(current_anim_state == 4)
		current_anim_state = 1;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_0) // PA0 controla la pausa/reanudacion del mp3
	{
		if (AudioState == AUDIO_STATE_PLAY)
		{
			AudioState = AUDIO_STATE_PAUSE;
			SSD1306_Clear();
			SSD1306_DrawBitmap(0, 0, PAUSA, 128, 64, 1); // Se dibuja la imagen de pausa cuando AudioState pasa a valer "AUDIO_STATE_PAUSE" debido a PA0
			SSD1306_UpdateScreen();
		}

		if (AudioState == AUDIO_STATE_WAIT)
		{
			AudioState = AUDIO_STATE_RESUME;
		}
	}

	if (GPIO_Pin == GPIO_PIN_1) // PA1 se utiliza para reproducir el siguiente archivo
	{
		AudioState = AUDIO_STATE_NEXT;
		change_animation();     // Al cambiar la cancion, se cambia la animacion mostrada
		frame_anim = 1;         // Se reinicia el frame de la animacion al primer frame
	}

	if (GPIO_Pin == GPIO_PIN_2) // PA2 se utiliza para reproducir el archivo anterior
	{
		AudioState = AUDIO_STATE_PREVIOUS;
		change_animation();     // Al cambiar la cancion, se cambia la animacion mostrada
		frame_anim = 1;         // Se reinicia el frame de la animacion al primer frame
	}
}

// Se utiliza un temporizador basico para la actualizacion de los frames en las animaciones para el OLED y ademas para reiniciar la conversion ADC
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)  // Se genera una interrupcion en intervalos inferiores a medio segundo.
{
	HAL_ADC_Start_IT(&hadc1); // Reiniciar conversion del ADC para que no se pare de controlar el volumen

    if (htim->Instance == TIM10 && (AudioState == AUDIO_STATE_PLAY)) // Solo se ejecutan las animaciones cuando el estado del audio es AUDIO_STATE_PLAY
    {
    	if(current_anim_state == 1)
    	{
			frame_anim++;
			if (frame_anim == 11) frame_anim = 1;
			animacion1(frame_anim);
    	}

    	else if(current_anim_state == 2)
    	{
			frame_anim++;
			if (frame_anim == 7) frame_anim = 1;
			animacion2(frame_anim);
    	}

    	else if(current_anim_state == 3)
    	{
			frame_anim++;
			if (frame_anim == 9) frame_anim = 1;
			animacion3(frame_anim);
    	}

    	else if(current_anim_state == 4)
    	{
			frame_anim++;
			if (frame_anim == 7) frame_anim = 1;
			animacion4(frame_anim);
    	}
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//**********************************ANIMACIONES*******************************************
// En todas las animaciones se obtiene el nombre de la cancion reproducida y se procede a escribirlo en la pantalla en todos los frames
// Tambien se representa el volumen actual de la musica en cada frame de todas las animaciones
char* get_name();
char* quitar_extension(const char*);
void animacion1(uint8_t frame)
{
	int x = 35; // Posicion del nombre de la cancion en el OLED
	char* song_name = quitar_extension(get_name()); // Usamos las dos funciones que hemos incluido a waveplayer.c para obtener el nombre del archivo en ejecucion

	  //// ANIMATION STARTS //////
	if (frame == 1)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10); // goto 0, 10
	    SSD1306_Puts ("\\O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 2)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("\\O", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |\\", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("\\ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 3)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts (" O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts ("\\|", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 4)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("\\O", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |\\", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}


	else if (frame == 5)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts (" O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts ("\\|", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("\\ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 6)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("\\O", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |\\", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 7)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts (" O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts ("\\|", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 8)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("\\O_", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 9)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("\\O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("\\ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if(frame == 10)
	{
	    SSD1306_Clear();
	    SSD1306_GotoXY (0,10);
	    SSD1306_Puts ("_O/", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 30);
	    SSD1306_Puts (" |", &Font_11x18, 1);
	    SSD1306_GotoXY (0, 45);
	    SSD1306_Puts ("/ \\", &Font_11x18, 1);
	    SSD1306_GotoXY (x, 30);
	    SSD1306_Puts (song_name, &Font_11x18, 0);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}
}

void animacion2(uint8_t frame)
{
	char* song_name = quitar_extension(get_name()); // Usamos las dos funciones que hemos incluido a waveplayer.c para obtener el nombre del archivo en ejecucion

	  //// ANIMATION STARTS //////
	if (frame == 1)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater1, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 2)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 3)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater3, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 4)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater4, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}


	else if (frame == 5)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater5, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 6)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, skater6, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}
}

void animacion3(uint8_t frame)
{
	char* song_name = quitar_extension(get_name()); // Usamos las dos funciones que hemos incluido a waveplayer.c para obtener el nombre del archivo en ejecucion

	  //// ANIMATION STARTS //////
	if (frame == 1)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador1, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 2)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 3)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador3, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 4)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador4, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}


	else if (frame == 5)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador3, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 6)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 7)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador4, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 8)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, fumador2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}
}

void animacion4(uint8_t frame)
{
	char* song_name = quitar_extension(get_name()); // Usamos las dos funciones que hemos incluido a waveplayer.c para obtener el nombre del archivo en ejecucion

	  //// ANIMATION STARTS //////
	if (frame == 1)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora1, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 2)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
	    SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 3)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora3, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 4)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora4, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}


	else if (frame == 5)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora3, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}

	else if (frame == 6)
	{
		SSD1306_Clear();
		SSD1306_DrawBitmap(0, 0, bailadora2, 128, 64, 1);
		SSD1306_GotoXY (10,45);
		SSD1306_Puts (song_name, &Font_11x18, 1);
	    SSD1306_GotoXY (90,5); SSD1306_Puts (buf_volumen, &Font_11x18, 1);
		SSD1306_UpdateScreen(); // update screen
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_FATFS_Init();
  MX_USB_HOST_Init();
  MX_I2C2_Init();
  MX_TIM10_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  SSD1306_Init();
  HAL_TIM_Base_Start_IT(&htim10);
  HAL_ADC_Start_IT(&hadc1);

  // ANTES DE ENTRAR AL BUCLE WHILE, GENERAMOS UN PAR DE IMAGENES DE PRESENTACION

  // 1. Dibujamos en el OLED la primera PORTADA durante 5 segundos
  SSD1306_DrawBitmap(0, 0, PORTADA1, 128, 64, 1);
  SSD1306_UpdateScreen(); // update screen
  HAL_Delay (5000);

  SSD1306_Clear();

  // 2. Dibujamos en el OLED la segunda PORTADA durante 5 segundos
  SSD1306_DrawBitmap(0, 0, PORTADA2, 128, 64, 1);
  SSD1306_UpdateScreen(); // update screen
  HAL_Delay (5000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    if (Appli_state == APPLICATION_READY) // TRUE cuando este el pendrive conectado y todo configurado correctamente
    {
    	Mount_USB();
    	AUDIO_PLAYER_Start(0);
    	while (!IsFinished) // Mientras se ejecuta este bucle, el mp3 esta en funcionamiento, pudiendo pausarse o cambiar las canciones
    	{
    		AUDIO_PLAYER_Process(TRUE, volumen); // Le mandamos al procesador el volumen actual que se desea, para que dentro de esa funcion, se actualice correctamente

    		if (AudioState == AUDIO_STATE_STOP)
    		{
    			IsFinished = 1;
    		}
    	}
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_8B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_44K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 48000;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 800;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
