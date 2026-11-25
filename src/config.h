// Ligação e calibração do visor ST7735 de 96x54.
#pragma once

// SPI de 4 fios. O visor não tem CS nem RST ligados à placa: CS fica preso em
// nível baixo no módulo, e por isso o barramento precisa do modo 3.
#define LCD_PIN_SCL 6
#define LCD_PIN_SDA 5
#define LCD_PIN_DC 7
#define LCD_SPI_HZ 10000000

// A área visível é um recorte da memória de 128x160 do controlador.
#define LCD_COL_OFFSET 16
#define LCD_ROW_OFFSET 106
#define LCD_MIRROR 0xC0
#define LCD_BGR 1
#define LCD_INVERT 0

// 1 desenha a imagem de cabeça para baixo, para a placa montada invertida.
#define LCD_ROTATE_180 1
