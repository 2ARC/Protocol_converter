/*
 * Simulador do CLP
 * 2023
 */
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <time.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
// Definições para o protocolo USS
#define LGE   1
#define ADR   2
#define PKE1  3
#define PKE2  4
#define PKE3  5
#define PKE4  6
#define PZD1  7
#define PZD2  8
#define PZD3  9
#define PZD4 10
#define PZD5 11
#define PZD6 12
#define PZD7 13
#define PZD8 14
using namespace std;
int chaveOutPut = 0;
int tempo = 30;
// Conta clock ticks
static inline unsigned long long
rdtsc ()
{
  unsigned hi, lo;
  asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
}
// Gera atraso com precisÃ£o limitada ao clock do processador
void
delay (double milisec)
{
  double a, b;
  a = b = rdtsc ();
  while (((b - a) / CLOCKS_PER_SEC / 1000) <= milisec)
    b = rdtsc ();
}
// Posiciona cursor
void
gotoxy (int x, int y)
{
  COORD posicao;
  HANDLE saidaH;
  saidaH = GetStdHandle (STD_OUTPUT_HANDLE);
  posicao.X = x - 1;
  posicao.Y = y - 1;
  SetConsoleCursorPosition (saidaH, posicao);
}
// Limpa a tela
void
clrscr ()
{
  COORD posicao;
  DWORD escreve;
  CONSOLE_SCREEN_BUFFER_INFO info;
  HANDLE saidaH;
  saidaH = GetStdHandle (STD_OUTPUT_HANDLE);
  posicao.X = 0;
  posicao.Y = 0;
  GetConsoleScreenBufferInfo (saidaH, &info);
  FillConsoleOutputCharacter (saidaH, ' ', info.dwSize.X * info.dwSize.Y,
			      posicao, &escreve);
  gotoxy (1, 1);
}
// Coloriza o texto
void
textcolor (int cor)
{
  HANDLE saidaH;
  saidaH = GetStdHandle (STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute (saidaH, cor);
}
// Converte caracteres de controle para mneumônicos da tabela Ascii
string toAscii(unsigned char dado[1],int chave){
     string str;
     if (dado[0] == 0x00) if (chave == 1) str = "NUL "; else str = "00 ";
     if (dado[0] == 0x01) if (chave == 1) str = "SOH "; else str = "01 ";
     if (dado[0] == 0x02) if (chave == 1) str = "STX "; else str = "02 ";
     if (dado[0] == 0x03) if (chave == 1) str = "ETX "; else str = "03 ";
     if (dado[0] == 0x04) if (chave == 1) str = "EOT "; else str = "04 ";
     if (dado[0] == 0x05) if (chave == 1) str = "ENQ "; else str = "05 ";
     if (dado[0] == 0x06) if (chave == 1) str = "ACK "; else str = "06 ";
     if (dado[0] == 0x07) if (chave == 1) str = "BEL "; else str = "07 ";
     if (dado[0] == 0x08) if (chave == 1) str = "BS  "; else str = "08 ";
     if (dado[0] == 0x09) if (chave == 1) str = "HT  "; else str = "09 ";
     if (dado[0] == 0x0A) if (chave == 1) str = "LF  "; else str = "0A ";
     if (dado[0] == 0x0B) if (chave == 1) str = "VT  "; else str = "0B ";
     if (dado[0] == 0x0C) if (chave == 1) str = "FF  "; else str = "0C ";
     if (dado[0] == 0x0D) if (chave == 1) str = "CR  "; else str = "0D ";
     if (dado[0] == 0x0E) if (chave == 1) str = "SO  "; else str = "0E ";
     if (dado[0] == 0x0F) if (chave == 1) str = "SI  "; else str = "0F ";
     if (dado[0] == 0x10) if (chave == 1) str = "DLE "; else str = "10 ";
     if (dado[0] == 0x11) if (chave == 1) str = "D1  "; else str = "11 ";
     if (dado[0] == 0x12) if (chave == 1) str = "D2  "; else str = "12 ";
     if (dado[0] == 0x13) if (chave == 1) str = "D3  "; else str = "13 ";
     if (dado[0] == 0x14) if (chave == 1) str = "D4  "; else str = "14 ";
     if (dado[0] == 0x15) if (chave == 1) str = "NAK "; else str = "15 ";
     if (dado[0] == 0x16) if (chave == 1) str = "SYN "; else str = "16 ";
     if (dado[0] == 0x17) if (chave == 1) str = "ETB "; else str = "17 ";
     if (dado[0] == 0x18) if (chave == 1) str = "CAN "; else str = "18 ";
     if (dado[0] == 0x19) if (chave == 1) str = "EOM "; else str = "19 ";
     if (dado[0] == 0x1A) if (chave == 1) str = "SUB "; else str = "1A ";
     if (dado[0] == 0x1B) if (chave == 1) str = "ESC "; else str = "1B ";
     if (dado[0] == 0x1C) if (chave == 1) str = "FS  "; else str = "1C ";
     if (dado[0] == 0x1D) if (chave == 1) str = "GS  "; else str = "1D ";
     if (dado[0] == 0x1E) if (chave == 1) str = "RS  "; else str = "1E ";
     if (dado[0] == 0x1F) if (chave == 1) str = "US  "; else str = "1F ";
     if (dado[0] == 0x20) if (chave == 1) str = "SPC "; else str = "20 ";
     if (dado[0] == 0x21) str = "21 ";
     if (dado[0] == 0x22) str = "22 ";
     if (dado[0] == 0x23) str = "23 ";
     if (dado[0] == 0x24) str = "24 ";
     if (dado[0] == 0x25) str = "25 ";
     if (dado[0] == 0x26) str = "26 ";
     if (dado[0] == 0x27) str = "27 ";
     if (dado[0] == 0x28) str = "28 ";
     if (dado[0] == 0x29) str = "29 ";
     if (dado[0] == 0x2A) str = "2A ";
     if (dado[0] == 0x2B) str = "2B ";
     if (dado[0] == 0x2C) str = "2C ";
     if (dado[0] == 0x2D) str = "2D ";
     if (dado[0] == 0x2E) str = "2E ";
     if (dado[0] == 0x2F) str = "2F ";
     if (dado[0] == 0x30) str = "30 ";
     if (dado[0] == 0x31) str = "31 ";
     if (dado[0] == 0x32) str = "32 ";
     if (dado[0] == 0x33) str = "33 ";
     if (dado[0] == 0x34) str = "34 ";
     if (dado[0] == 0x35) str = "35 ";
     if (dado[0] == 0x36) str = "36 ";
     if (dado[0] == 0x37) str = "37 ";
     if (dado[0] == 0x38) str = "38 ";
     if (dado[0] == 0x39) str = "39 ";
     if (dado[0] == 0x3A) str = "3A ";
     if (dado[0] == 0x3B) str = "3B ";
     if (dado[0] == 0x3C) str = "3C ";
     if (dado[0] == 0x3D) str = "3D ";
     if (dado[0] == 0x3E) str = "3E ";
     if (dado[0] == 0x3F) str = "3F ";
     if (dado[0] == 0x40) str = "40 ";
     if (dado[0] == 0x41) str = "41 ";
     if (dado[0] == 0x42) str = "42 ";
     if (dado[0] == 0x43) str = "43 ";
     if (dado[0] == 0x44) str = "44 ";
     if (dado[0] == 0x45) str = "45 ";
     if (dado[0] == 0x46) str = "46 ";
     if (dado[0] == 0x47) str = "47 ";
     if (dado[0] == 0x48) str = "48 ";
     if (dado[0] == 0x49) str = "49 ";
     if (dado[0] == 0x4A) str = "4A ";
     if (dado[0] == 0x4B) str = "4B ";
     if (dado[0] == 0x4C) str = "4C ";
     if (dado[0] == 0x4D) str = "4D ";
     if (dado[0] == 0x4E) str = "4E ";
     if (dado[0] == 0x4F) str = "4F ";
     if (dado[0] == 0x50) str = "50 ";
     if (dado[0] == 0x51) str = "51 ";
     if (dado[0] == 0x52) str = "52 ";
     if (dado[0] == 0x53) str = "53 ";
     if (dado[0] == 0x54) str = "54 ";
     if (dado[0] == 0x55) str = "55 ";
     if (dado[0] == 0x56) str = "56 ";
     if (dado[0] == 0x57) str = "57 ";
     if (dado[0] == 0x58) str = "58 ";
     if (dado[0] == 0x59) str = "59 ";
     if (dado[0] == 0x5A) str = "5A ";
     if (dado[0] == 0x5B) str = "5B ";
     if (dado[0] == 0x5C) str = "5C ";
     if (dado[0] == 0x5D) str = "5D ";
     if (dado[0] == 0x5E) str = "5E ";
     if (dado[0] == 0x5F) str = "5F ";
     if (dado[0] == 0x60) str = "60 ";
     if (dado[0] == 0x61) str = "61 ";
     if (dado[0] == 0x62) str = "62 ";
     if (dado[0] == 0x63) str = "63 ";
     if (dado[0] == 0x64) str = "64 ";
     if (dado[0] == 0x65) str = "65 ";
     if (dado[0] == 0x66) str = "66 ";
     if (dado[0] == 0x67) str = "67 ";
     if (dado[0] == 0x68) str = "68 ";
     if (dado[0] == 0x69) str = "69 ";
     if (dado[0] == 0x6A) str = "6A ";
     if (dado[0] == 0x6B) str = "6B ";
     if (dado[0] == 0x6C) str = "6C ";
     if (dado[0] == 0x6D) str = "6D ";
     if (dado[0] == 0x6E) str = "6E ";
     if (dado[0] == 0x6F) str = "6F ";
     if (dado[0] == 0x70) str = "70 ";
     if (dado[0] == 0x71) str = "71 ";
     if (dado[0] == 0x72) str = "72 ";
     if (dado[0] == 0x73) str = "73 ";
     if (dado[0] == 0x74) str = "74 ";
     if (dado[0] == 0x75) str = "75 ";
     if (dado[0] == 0x76) str = "76 ";
     if (dado[0] == 0x77) str = "77 ";
     if (dado[0] == 0x78) str = "78 ";
     if (dado[0] == 0x79) str = "79 ";
     if (dado[0] == 0x7A) str = "7A ";
     if (dado[0] == 0x7B) str = "7B ";
     if (dado[0] == 0x7C) str = "7C ";
     if (dado[0] == 0x7D) str = "7D ";
     if (dado[0] == 0x7E) str = "7E ";
     if (dado[0] == 0x7F) if (chave == 1) str = "DEL "; else str = "7F ";
     if (dado[0] == 0x80) str = "80 ";
     if (dado[0] == 0x81) str = "81 ";
     if (dado[0] == 0x82) str = "82 ";
     if (dado[0] == 0x83) str = "83 ";
     if (dado[0] == 0x84) str = "84 ";
     if (dado[0] == 0x85) str = "85 ";
     if (dado[0] == 0x86) str = "86 ";
     if (dado[0] == 0x87) str = "87 ";
     if (dado[0] == 0x88) str = "88 ";
     if (dado[0] == 0x89) str = "89 ";
     if (dado[0] == 0x8A) str = "8A ";
     if (dado[0] == 0x8B) str = "8B ";
     if (dado[0] == 0x8C) str = "8C ";
     if (dado[0] == 0x8D) str = "8D ";
     if (dado[0] == 0x8E) str = "8E ";
     if (dado[0] == 0x8F) str = "8F ";
     if (dado[0] == 0x90) str = "90 ";
     if (dado[0] == 0x91) str = "91 ";
     if (dado[0] == 0x92) str = "92 ";
     if (dado[0] == 0x93) str = "93 ";
     if (dado[0] == 0x94) str = "94 ";
     if (dado[0] == 0x95) str = "95 ";
     if (dado[0] == 0x96) str = "96 ";
     if (dado[0] == 0x97) str = "97 ";
     if (dado[0] == 0x98) str = "98 ";
     if (dado[0] == 0x99) str = "99 ";
     if (dado[0] == 0x9A) str = "9A ";
     if (dado[0] == 0x9B) str = "9B ";
     if (dado[0] == 0x9C) str = "9C ";
     if (dado[0] == 0x9D) str = "9D ";
     if (dado[0] == 0x9E) str = "9E ";
     if (dado[0] == 0x9F) str = "9F ";
     if (dado[0] == 0xA0) str = "A0 ";
     if (dado[0] == 0xA1) str = "A1 ";
     if (dado[0] == 0xA2) str = "A2 ";
     if (dado[0] == 0xA3) str = "A3 ";
     if (dado[0] == 0xA4) str = "A4 ";
     if (dado[0] == 0xA5) str = "A5 ";
     if (dado[0] == 0xA6) str = "A6 ";
     if (dado[0] == 0xA7) str = "A7 ";
     if (dado[0] == 0xA8) str = "A8 ";
     if (dado[0] == 0xA9) str = "A9 ";
     if (dado[0] == 0xAA) str = "AA ";
     if (dado[0] == 0xAB) str = "AB ";
     if (dado[0] == 0xAC) str = "AC ";
     if (dado[0] == 0xAD) str = "AD ";
     if (dado[0] == 0xAE) str = "AE ";
     if (dado[0] == 0xAF) str = "AF ";
     if (dado[0] == 0xB0) str = "B0 ";
     if (dado[0] == 0xB1) str = "B1 ";
     if (dado[0] == 0xB2) str = "B2 ";
     if (dado[0] == 0xB3) str = "B3 ";
     if (dado[0] == 0xB4) str = "B4 ";
     if (dado[0] == 0xB5) str = "B5 ";
     if (dado[0] == 0xB6) str = "B6 ";
     if (dado[0] == 0xB7) str = "B7 ";
     if (dado[0] == 0xB8) str = "B8 ";
     if (dado[0] == 0xB9) str = "B9 ";
     if (dado[0] == 0xBA) str = "BA ";
     if (dado[0] == 0xBB) str = "BB ";
     if (dado[0] == 0xBC) str = "BC ";
     if (dado[0] == 0xBD) str = "BD ";
     if (dado[0] == 0xBE) str = "BE ";
     if (dado[0] == 0xBF) str = "BF ";
     if (dado[0] == 0xC0) str = "C0 ";
     if (dado[0] == 0xC1) str = "C1 ";
     if (dado[0] == 0xC2) str = "C2 ";
     if (dado[0] == 0xC3) str = "C3 ";
     if (dado[0] == 0xC4) str = "C4 ";
     if (dado[0] == 0xC5) str = "C5 ";
     if (dado[0] == 0xC6) str = "C6 ";
     if (dado[0] == 0xC7) str = "C7 ";
     if (dado[0] == 0xC8) str = "C8 ";
     if (dado[0] == 0xC9) str = "C9 ";
     if (dado[0] == 0xCA) str = "CA ";
     if (dado[0] == 0xCB) str = "CB ";
     if (dado[0] == 0xCC) str = "CC ";
     if (dado[0] == 0xCD) str = "CD ";
     if (dado[0] == 0xCE) str = "CE ";
     if (dado[0] == 0xCF) str = "CF ";
     if (dado[0] == 0xD0) str = "D0 ";
     if (dado[0] == 0xD1) str = "D1 ";
     if (dado[0] == 0xD2) str = "D2 ";
     if (dado[0] == 0xD3) str = "D3 ";
     if (dado[0] == 0xD4) str = "D4 ";
     if (dado[0] == 0xD5) str = "D5 ";
     if (dado[0] == 0xD6) str = "D6 ";
     if (dado[0] == 0xD7) str = "D7 ";
     if (dado[0] == 0xD8) str = "D8 ";
     if (dado[0] == 0xD9) str = "D9 ";
     if (dado[0] == 0xDA) str = "DA ";
     if (dado[0] == 0xDB) str = "DB ";
     if (dado[0] == 0xDC) str = "DC ";
     if (dado[0] == 0xDD) str = "DD ";
     if (dado[0] == 0xDE) str = "DE ";
     if (dado[0] == 0xDF) str = "DF ";
     if (dado[0] == 0xE0) str = "E0 ";
     if (dado[0] == 0xE1) str = "E1 ";
     if (dado[0] == 0xE2) str = "E2 ";
     if (dado[0] == 0xE3) str = "E3 ";
     if (dado[0] == 0xE4) str = "E4 ";
     if (dado[0] == 0xE5) str = "E5 ";
     if (dado[0] == 0xE6) str = "E6 ";
     if (dado[0] == 0xE7) str = "E7 ";
     if (dado[0] == 0xE8) str = "E8 ";
     if (dado[0] == 0xE9) str = "E9 ";
     if (dado[0] == 0xEA) str = "EA ";
     if (dado[0] == 0xEB) str = "EB ";
     if (dado[0] == 0xEC) str = "EC ";
     if (dado[0] == 0xED) str = "ED ";
     if (dado[0] == 0xEE) str = "EE ";
     if (dado[0] == 0xEF) str = "EF ";
     if (dado[0] == 0xF0) str = "F0 ";
     if (dado[0] == 0xF1) str = "F1 ";
     if (dado[0] == 0xF2) str = "F2 ";
     if (dado[0] == 0xF3) str = "F3 ";
     if (dado[0] == 0xF4) str = "F4 ";
     if (dado[0] == 0xF5) str = "F5 ";
     if (dado[0] == 0xF6) str = "F6 ";
     if (dado[0] == 0xF7) str = "F7 ";
     if (dado[0] == 0xF8) str = "F8 ";
     if (dado[0] == 0xF9) str = "F9 ";
     if (dado[0] == 0xFA) str = "FA ";
     if (dado[0] == 0xFB) str = "FB ";
     if (dado[0] == 0xFC) str = "FC ";
     if (dado[0] == 0xFD) str = "FD ";
     if (dado[0] == 0xFE) str = "FE ";
     if (dado[0] == 0xFF) str = "FF ";
     return str;
}
// Telegrama teste
unsigned char telegrama[17] =
    { 0 };
unsigned char STX[1] =
    { 0 };
unsigned char BCC[1] =
    { 0 };
int numeroDoEnderecoInicial = 1;
int numeroDoEnderecoFinal = 5;
int opcao = 1;
int resposta = 0;
int comprimento = 14; // Seta para o inversor legado
int contador = 1;
int contadorMaximo = 63; // Seta para o CLP
int quantidade = 1; // Quantidade de parâmetros transmitidos (V20 = r2024)
int endereco = 1;
// Saída para o arquivo de log
ofstream INV1;
ofstream INV2;
ofstream INV3;
ofstream INV4;
ofstream INV5;
// Mostra na tela e gera de log
void
registraParametros (int envia, int contrato)
{
  int cor;
  if (contrato == 1)
    {
      if (envia)
	{
	  // Mostra telegrama no console, alterna cor nos dados
	  textcolor (15);
	  cout << toAscii (&telegrama[0], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[1], 0) << "";
	  textcolor (3);
	  cout << toAscii (&telegrama[ADR], 0) << "";
	  textcolor (9);
	  cout << toAscii (&telegrama[3], 0) << "";
	  textcolor (9);
	  cout << toAscii (&telegrama[4], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[5], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[6], 0) << "";
	  if ((telegrama[7] == 0x00) && (telegrama[8] == 0x00))
	    cor = 15;
	  else
	    cor = 14;
	  textcolor (cor);
	  cout << toAscii (&telegrama[7], 0) << "";
	  textcolor (cor);
	  cout << toAscii (&telegrama[8], 0) << "";
	  textcolor (11);
	  cout << toAscii (&telegrama[PZD3], 0) << "";
	  textcolor (11);
	  cout << toAscii (&telegrama[PZD4], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[11], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[12], 0) << "";
	  textcolor (2);
	  cout << toAscii (&telegrama[13], 0) << "";
	  cout << endl;
	}
      else
	{
	  textcolor (15);
	  cout << toAscii (&telegrama[0], 0) << "";
	  cout << toAscii (&telegrama[1], 0) << "";
	  cout << toAscii (&telegrama[ADR], 0) << "";
	  if ((telegrama[3] == 0x10))
	    cor = 13;
	  else
	    cor = 9;
	  textcolor (cor);
	  cout << toAscii (&telegrama[3], 0) << "";
	  cout << toAscii (&telegrama[4], 0) << "";
	  textcolor (15);
	  cout << toAscii (&telegrama[5], 0) << "";
	  cout << toAscii (&telegrama[6], 0) << "";
	  cout << toAscii (&telegrama[7], 0) << "";
	  cout << toAscii (&telegrama[8], 0) << "";
	  cout << toAscii (&telegrama[PZD3], 0) << "";
	  cout << toAscii (&telegrama[PZD4], 0) << "";
	  cout << toAscii (&telegrama[11], 0) << "";
	  cout << toAscii (&telegrama[12], 0) << "";
	  cout << toAscii (&telegrama[13], 0) << "";
	  cout << endl;
	}
    }
  // Transfere bytes capturados para o arquivo de log
  for (int i = 0; i < comprimento; i++)
    {
      switch (telegrama[ADR])
      {
	case 1:
	  {
	    INV1 << toAscii (&telegrama[i], 0);
	    break;
	  }
	case 2:
	  {
	    INV2 << toAscii (&telegrama[i], 0);
	    break;
	  }
	case 3:
	  {
	    INV3 << toAscii (&telegrama[i], 0);
	    break;
	  }
	case 4:
	  {
	    INV4 << toAscii (&telegrama[i], 0);
	    break;
	  }
	case 5:
	  {
	    INV5 << toAscii (&telegrama[i], 0);
	    break;
	  }
      }
    }
  switch (telegrama[ADR])
  {
    case 1:
      {
	INV1 << endl;
	break;
      }
    case 2:
      {
	INV2 << endl;
	break;
      }
    case 3:
      {
	INV3 << endl;
	break;
      }
    case 4:
      {
	INV4 << endl;
	break;
      }
    case 5:
      {
	INV5 << endl;
	break;
      }
  }
}
// Lista com telegramas para simulação
void
telegramasCLP (void)
{
  telegrama[0] = 0x02;
  telegrama[1] = 0x0C;
  telegrama[5] = 0x00;
  telegrama[6] = 0x00;
  telegrama[9] = 0x14;
  telegrama[PZD4] = 0x7E;
  telegrama[11] = 0x00;
  telegrama[12] = 0x00;
  telegrama[ADR] = endereco;
  switch (contador)
  {
    case 1: // P930
      {
	telegrama[3] = 0x13;
	telegrama[4] = 0xA2;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 2:
      {
	telegrama[3] = 0x00;
	telegrama[4] = 0x00;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 3:
      {
	telegrama[3] = 0x10;
	telegrama[4] = 0x00;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 4: // P930
      {
	telegrama[3] = 0x13;
	telegrama[4] = 0xA2;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 5: // P001
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x01;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 6: // P002 => P1120
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x02;
	telegrama[7] = 0x00;
	telegrama[8] = 0x0A;
	break;
      }
    case 7: // P003 => P1121
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x03;
	telegrama[7] = 0x00;
	telegrama[8] = 0x0A;
	break;
      }
    case 8:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x04;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 9: // P006 => P1000
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x06;
	telegrama[7] = 0x00;
	telegrama[8] = 0x02;
	break;
      }
    case 10: // P007 => P0700
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x07;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 11:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x09;
	telegrama[7] = 0x00;
	telegrama[8] = 0x03;
	break;
      }
    case 12:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x0B;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 13:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x0C;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 14:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x0D;
	telegrama[7] = 0x04;
	telegrama[8] = 0xA4;
	break;
      }
    case 15:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x0E;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 16:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x0F;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 17:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x10;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 18:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x12;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 19: // P033 JOG ramp up
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x21;
	switch (telegrama[ADR])
	{
	  case 1:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x00;
	      break;
	    }
	  case 2:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x04;
	      break;
	    }
	  case 3:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x04;
	      break;
	    }
	  case 4:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x0A;
	      break;
	    }
	  case 5:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x0A;
	      break;
	    }
	}
	break;
      }
    case 20: // P034 JOG ramp down
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x22;
	switch (telegrama[ADR])
	{
	  case 1:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x00;
	      break;
	    }
	  case 2:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x04;
	      break;
	    }
	  case 3:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x04;
	      break;
	    }
	  case 4:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x0A;
	      break;
	    }
	  case 5:
	    {
	      telegrama[7] = 0x00;
	      telegrama[8] = 0x0A;
	      break;
	    }
	}
	break;
      }
    case 21: // P041 = P1001 = frequency set point
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x29;
	switch (telegrama[ADR])
	{
	  case 1:
	    {
	      telegrama[7] = 0x02;
	      telegrama[8] = 0x06; // 51.8
	      break;
	    }
	  case 2:
	    {
	      telegrama[7] = 0x02;
	      telegrama[8] = 0x0C; // 52.4
	      break;
	    }
	  case 3:
	    {
	      telegrama[7] = 0x02;
	      telegrama[8] = 0x0B; // 52.3
	      break;
	    }
	  case 4:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x23; // 29.1
	      break;
	    }
	  case 5:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x2B; // 29.9
	      break;
	    }
	}
	break;
      }
    case 22:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2A;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 23:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2B;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 24:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2C;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 25:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2D;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 26:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2E;
	telegrama[7] = 0x00;
	switch (telegrama[ADR])
	{
	  case 1:
	    {
	      telegrama[7] = 0x02;
	      telegrama[8] = 0x06; // 51.8
	      break;
	    }
	  case 2:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x26; // 29.4
	      break;
	    }
	  case 3:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x26; // 29.4
	      break;
	    }
	  case 4:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x21; // 28.9
	      break;
	    }
	  case 5:
	    {
	      telegrama[7] = 0x01;
	      telegrama[8] = 0x27; // 29.5
	      break;
	    }
	}
	break;
      }
    case 27:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x2F;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 28:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x30;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 29:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x31;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 30:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x32;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 31:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x33;
	telegrama[7] = 0x00;
	if (telegrama[ADR] != 0x04)
	  {
	    telegrama[8] = 0x01;
	  }
	break;
      }
    case 32:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x34;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 33:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x35;
	telegrama[7] = 0x00;
	telegrama[8] = 0x11;
	break;
      }
    case 34:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x36;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 35:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x37;
	telegrama[7] = 0x00;
	telegrama[8] = 0x10;
	break;
      }
    case 36:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x38;
	telegrama[7] = 0x00;
	telegrama[8] = 0x02;
	break;
      }
    case 37:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x3D;
	telegrama[7] = 0x00;
	telegrama[8] = 0x01;
	break;
      }
    case 38:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x3E;
	telegrama[7] = 0x00;
	telegrama[8] = 0x06;
	break;
      }
    case 39:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x47;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 40:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x49;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 41:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4A;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 42:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4B;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 43:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4C;
	telegrama[7] = 0x00;
	telegrama[8] = 0x08;
	break;
      }
    case 44:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4D;
	telegrama[7] = 0x00;
	telegrama[8] = 0x01;
	break;
      }
    case 45:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4E;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 46:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x4F;
	telegrama[7] = 0x00;
	telegrama[8] = 0x14;
	break;
      }
    case 47: // P081 => P0310
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x51;
	telegrama[7] = 0x01;
	telegrama[8] = 0xF4;
	break;
      }
    case 48: // P082 => P0311
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x52;
	telegrama[7] = 0x03;
	telegrama[8] = 0x84;
	break;
      }
    case 49: // P083 => P0305
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x53;
	telegrama[7] = 0x00;
	telegrama[8] = 0x34;
	break;
      }
    case 50: // P084 => P0304
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x54;
	telegrama[7] = 0x00;
	telegrama[8] = 0xE6;
	break;
      }
    case 51: // P085 => P0307
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x55;
	telegrama[7] = 0x00;
	telegrama[8] = 0x6E;
	break;
      }
    case 52: // P086 => P0640
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x56;
	telegrama[7] = 0x00;
	telegrama[7] = 0x00;
	telegrama[8] = 0xC4;
	break;
      }
    case 53:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x57;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 54:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x58;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 55: // P089 => P0350
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x59;
	telegrama[7] = 0x02;
	telegrama[8] = 0x6C;
	break;
      }
    case 56:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x5D;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 57:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x5F;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 58:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x65;
	telegrama[7] = 0x00;
	telegrama[8] = 0x01;
	break;
      }
    case 59:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x79;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 60:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x7A;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 61:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x7B;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
    case 62:
      {
	telegrama[3] = 0x20;
	telegrama[4] = 0x7C;
	telegrama[7] = 0x00;
	telegrama[8] = 0x00;
	break;
      }
  }
}
// Função principal
int
main (void)
{
  // Cria arquivo de log
  char d;
  string infilename;
  infilename = "INV1.txt";
  INV1.open ("INV1.txt"), ios
      ::app;
  infilename = "INV2.txt";
  INV2.open ("INV2.txt"), ios
      ::app;
  infilename = "INV3.txt";
  INV3.open ("INV3.txt"), ios
      ::app;
  infilename = "INV4.txt";
  INV4.open ("INV4.txt"), ios
      ::app;
  infilename = "INV5.txt";
  INV5.open ("INV5.txt"), ios
      ::app;
  // Abre a porta serial
  HANDLE hSerial;
  char PORTA[5];
  opcao = 3; // Número da porta no Gerenciador de dispositivos
  if (opcao == 1)
    strcpy (PORTA, "com1");
  else if (opcao == 2)
    strcpy (PORTA, "com2");
  else if (opcao == 3)
    strcpy (PORTA, "com3");
  else if (opcao == 4)
    strcpy (PORTA, "com4");
  else if (opcao == 5)
    strcpy (PORTA, "com5");
  else if (opcao == 6)
    strcpy (PORTA, "com6");
  else if (opcao == 7)
    strcpy (PORTA, "com7");
  else if (opcao == 8)
    strcpy (PORTA, "com8");
  else if (opcao == 9)
    strcpy (PORTA, "com9");
  else
    strcpy (PORTA, "com1"); // Padrão porta 1
  hSerial = CreateFile (PORTA, GENERIC_READ | GENERIC_WRITE, 0, 0,
			OPEN_EXISTING, 0, 0);
  if (hSerial == INVALID_HANDLE_VALUE)
    {
      if (GetLastError () == ERROR_FILE_NOT_FOUND)
	{
	  //serial port does not exist. Inform user.
	}
      //some other error occurred. Inform user.
    }
  // Parâmetros de configuração da porta serial
  DCB dcbSerialParams =
      { 0 };
  dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
  if (!GetCommState (hSerial, &dcbSerialParams))
    {
      //error getting state
    }
  dcbSerialParams.BaudRate = CBR_9600;
  dcbSerialParams.ByteSize = 8;
  dcbSerialParams.StopBits = ONESTOPBIT;
  dcbSerialParams.Parity = EVENPARITY;
  if (!SetCommState (hSerial, &dcbSerialParams))
    {
      //error setting serial port state
    }
  // Tempos limite de operação, evita que o programa trave
  COMMTIMEOUTS timeouts =
      { 0 };
  timeouts.ReadIntervalTimeout = 30;
  timeouts.ReadTotalTimeoutConstant = 50;
  timeouts.ReadTotalTimeoutMultiplier = 1;
  timeouts.WriteTotalTimeoutConstant = 50;
  timeouts.WriteTotalTimeoutMultiplier = 1;
  if (!SetCommTimeouts (hSerial, &timeouts))
    {
      //error occureed. Inform user
    }
  // Tratamento de erro
  char lastError[1024];
  FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		 NULL,
		 GetLastError (), MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		 lastError, 1024,
		 NULL);
  DWORD dwBytesRead = 0;
  DWORD dwBytesTransferred;
  DWORD dwCommModemStatus;
  // Valores de contagem para o simulador
  comprimento = 14;
  contadorMaximo = 63;
  // O endereço é uma inteiro entre 0 e 31
  numeroDoEnderecoInicial = 1;
  endereco = numeroDoEnderecoInicial;
  numeroDoEnderecoFinal = 3;
  contador = 1;

  while (1)
    {
      if (tempo != 0)
	{
	  contador = 4;
	  telegramasCLP ();
	  endereco++;
	  if (endereco > numeroDoEnderecoFinal)
	    {
	      endereco = numeroDoEnderecoInicial;
	      endereco = 1;
	      quantidade++;
	    }
	  // Calcula BCC
	  telegrama[comprimento - 1] = 0x00;
	  for (int i = 0; i < (comprimento - 1); i++)
	    telegrama[comprimento - 1] ^= telegrama[i];
	  // Escreve na porta serial comprimento bytes
	  if (!WriteFile (hSerial, telegrama, comprimento, &dwBytesRead, NULL))
	    {
	      cout << "Erro na transmissao!" << endl;
	    }
	  if (telegrama[0] == 0x02)
	    registraParametros (1, opcao);
	  // Recebe
	  comprimento = 14;
	  for (int i = 0; i < comprimento; i++)
	    telegrama[i] = 0x00;
	  delay (30);
	  if (!ReadFile (hSerial, telegrama, comprimento, &dwBytesRead, NULL))
	    {
	      // Trata o erro
	    }
	  if (telegrama[0] == 0x02)
	    registraParametros (0, opcao);
	  delay (tempo);
	}
    }
  // Fecha a porta serial
  CloseHandle (hSerial);
  // Encerra e retorna ao sistema
  return 0;
}
