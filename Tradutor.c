/*
 * Tradutor.c
 * 2023
 * Quando não indicado, pág. se refere ao datasheet do Atmega162
*/
 
// Para alterar a frequência do mcu, também alterar:
// OCR1A = NOVOVALOR; UBRR0H = (NOVOVALOR >> 8); UBRR0L = NOVOVALOR;
// Este "#define" deve sempre estar antes de "#include <util/delay.h>"
#define F_CPU 11059200UL
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
/********************* Variáveis globais ********************************/
// Comprimento do telegrama setado inicialmente para valor do inversor legado (14 bytes)
// Já no V20, o comprimento varia entre 10 e 18, mas sempre inicia com 14
// para manter compatibilidade com o inversor legado. Este valor é alterado dinamicamente
// de acordo com a necessidade, durante a leitura do byte LGE, mas na tradução do inversor
// ele sempre será igual a 14 para o CLP
static volatile uint_fast8_t comprimentoTelegramaCLP = 14;
static volatile uint_fast8_t comprimentoTelegramaInversores = 14;
// Define mneumônicos de acordo com o protocolo USS
#define STX   0
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
#define BCCM (comprimentoTelegramaCLP - 1)
#define BCCE (comprimentoTelegramaInversores - 1)
static volatile uint_fast16_t parametro = 0;
static volatile unsigned char valorPKE1 = 0x00;
static volatile unsigned char valorPKE2 = 0x00;
static volatile unsigned char valorPZD1 = 0x00;
static volatile unsigned char valorPZD2 = 0x00;
void trataValorParametroDivisor(uint_fast8_t);
void converteParaIEEE754(uint_fast8_t);
/*********************** UART0 / Lado do CLP *********************************/
static volatile uint_fast8_t indiceTelegramaCLP = 0;
static volatile unsigned char telegramaMestre[21] = {0};
void traduzTelegramaCLP(void);
void enviaTelegramaCLP(void);
/************************ UART1 / Lado dos inversores *********************************/
static volatile uint_fast8_t indiceTelegramaInversores = 0;
static volatile unsigned char telegramaInversores[21] = {0};
void traduzTelegramaInversores(void);
void enviaTelegramaInversores(void);
/******* Seta bits de portas, timers, UARTs, dispara interrupções *******/
int main(void)
{
	// Cancela qualquer chamado de interrupçãoo para
	// setar o hardware com segurança
	cli();
	// Ajusta PORTA
	DDRA = 0x00;
	// Ajusta PORTB
	DDRB = 0xF8;
	// Ajusta PORTC
	DDRC = 0x00;
	// Ajusta PORTD
	DDRD = 0x26;
	// Ajusta PORTE
	DDRE = 0x00;
	// Configura heart beat por hardware, assim o led pisca sempre
	// a cada segundo, indicando que o mcu está trabalhando
	// págs. 112, 118-119, 128-131
	// Ajusta timer 1 para modo CTC, pág. 131
	TCCR1B |= (1 << WGM12);
	// Habilita timer 1 para alternar quando OCR1A
	// alcançaar limite predefinido, pag. 128
	TCCR1A |= (1 << COM1A0);
	// Define o limite, prescale de 256,
	// equivale a 1/2s, a 11.0592MHz, pág. 133
	OCR1A = 21600;
	// Inicia timer 1, prescale 256, pág. 132
	TCCR1B |= (1 << CS12);
	/*************************** Inicializa UART0 ***************************/
	// Habilita RXTX, pág. 187
	UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
	// Usa caracteres de 8 bits, even, totalizando 11 bits para transmissão, pág. 189-190
	// URSEL0 sempre tem de ser 1 para escrever no UCSR0C, pág. 189
	UCSR0C |= (1 << URSEL0 ) | (1 << UPM01) | (3 << UCSZ00);
	// Seta velocidade em BAUD, configura
	// dois registradores, UBRR0H e UBRR0L
	// 71, 9600bps a 11.0592MHz, pág. 193
	UBRR0H = (71 >> 8);
	UBRR0L = 71;
	// Habilita interrupção caso haja caractere no buffer de RX, pág. 187
	UCSR0B |= (1 << RXCIE0);
	/*************************** Inicializa UART1 ***************************/
	// Habilita RXTX, pág. 187
	UCSR1B |= (1 << RXEN1) | (1 << TXEN1);
	// Usa caracteres de 8 bits, even, totalizando 11 bits para transmissão, pág. 189-190
	// URSEL0 sempre tem de ser 1 para escrever no UCSR0C, pág. 189
	UCSR1C |= (1 << URSEL1) | (1 << UPM11) | (3 << UCSZ10);
	// Seta velocidade em BAUD, configura
	// dois registradores, UBRR0H e UBRR0L
	// 71, 9600bps a 11.0592MHz, pág. 193
	UBRR1H = (71 >> 8); // 5 para 115200 P2010 = 12
	UBRR1L = 71; // 5 para 115200 P2010 = 12
	// Habilita interrupção caso haja caractere no buffer de RX, pág. 187
	UCSR1B |= (1 << RXCIE1);
	// Habilita watchdog
	wdt_enable(WDTO_2S);
	// Habilita todas as interrupções
	sei();
	// Garantia durante reset do wathdog
	parametro = 0;
	valorPZD1 = 0x00;
	valorPZD2 = 0x00;
	valorPKE1 = 0x00;
	valorPKE2 = 0x00;
	indiceTelegramaCLP = 0;
	indiceTelegramaInversores = 0;
	comprimentoTelegramaCLP = 14;
	comprimentoTelegramaInversores = 14;
	// Reseta watchdog se enviaTelegramaCLP e enviaTelegramaInversores funcionam sequencialmente
	uint_fast8_t resetaWatchDog = 0;
	// Reseta watchdog
	wdt_reset();
	// Pisca lento = 0 (aguarda comunicação) pisca rápido = 1 (CLP comunicando com inversores)
	uint_fast8_t led = 0;
	// Laço principal
	for(;;)
	{
		// Verifica se já é o momento de traduzir e/ou enviar os telegramas
		if (indiceTelegramaCLP == comprimentoTelegramaCLP)
		{
			// Desabilita as interrupções
			cli();
			// OK, pode traduzir, se for um endereço de inversor V20
			if (((PINA & (1 << 7)) && (telegramaMestre[ADR] == 0x01)) ||
				((PINA & (1 << 6)) && (telegramaMestre[ADR] == 0x02)) ||
				((PINA & (1 << 5)) && (telegramaMestre[ADR] == 0x03)) ||
				((PINA & (1 << 4)) && (telegramaMestre[ADR] == 0x04)) ||
				((PINA & (1 << 3)) && (telegramaMestre[ADR] == 0x05)))
			{
				traduzTelegramaCLP();
			}
			// Envia o telegrama do jeito que está, traduzido ou não
			enviaTelegramaCLP();
			if (resetaWatchDog == 0)
			{
				resetaWatchDog = 1;
			}
			// Habilita as interrupções		
			sei();
		}
		// sem else
		// Executa os três if(s) em sequência, portanto não usar else
		if (indiceTelegramaInversores == comprimentoTelegramaInversores)
		{
			// Desabilita as interrupções
			cli();
			// OK, pode traduzir, se for um endereço de inversor V20
			if (((PINA & (1 << 7)) && (telegramaInversores[ADR] == 0x01)) ||
				((PINA & (1 << 6)) && (telegramaInversores[ADR] == 0x02)) ||
				((PINA & (1 << 5)) && (telegramaInversores[ADR] == 0x03)) ||
				((PINA & (1 << 4)) && (telegramaInversores[ADR] == 0x04)) ||
				((PINA & (1 << 3)) && (telegramaInversores[ADR] == 0x05)))
			{
				traduzTelegramaInversores();
			}
			// Envia o telegrama do jeito que está, traduzido ou não
			enviaTelegramaInversores();			
			if (resetaWatchDog == 1)
			{
				resetaWatchDog = 2;
			}
			// Habilita as interrupções
			sei();
		}
		// sem else
		if (resetaWatchDog == 2)
		{
			resetaWatchDog = 0;
			wdt_reset();
			if (led == 0)
			{
				led = 1;
				OCR1A = 5400; // 125 ms
				TCCR1B |= (1 << CS12); // Permanece rápido enquanto houver comunicação
			}
		}
	}
} // Fim de main
// Converte PZD para IEEE754
void converteParaIEEE754(uint_fast8_t divisor)
{
	// Word que vai manter PZD1 (MSB) e PZD2 (LSB), totalizando 16 bits
	// Pega PZD1 e PZD2 e converte para 1 word (16 bits)
	uint_fast16_t tmphex = (telegramaMestre[PZD1] << 8) | telegramaMestre[PZD2];
	// Float que vai manter os 4 bytes no formato IEEE754 na memória
	// O divisor é necessário pois o valor que o CLP envia para o inversor
	// é múltiplo de 10, e isso depende de cada parâmetro
	// Parâmetros com valores menores que 255 (ou 25.5), podem ser lidos pelo inversor legado com 1 byte na posição de PZD2
	// Parâmetros com valores de 0 até (acima) de 255 (0 - 650, por ex.) são lidos com 2 bytes, em PZD1 e PZD2
	// Se, por ex., for um parâmetro de escala de tempo, é dividido por 10, se de frequência, dividido por 100
	float tmpfloat = (float)((float)tmphex / (float)divisor);
	unsigned char* ptr = (unsigned char*) &tmpfloat; // Pega o conteúdo no endereço do float
	// Matriz que vai receber os bytes
	unsigned char pzd[4];
	// Coloque os 4 bytes do LSB para o MSB
	for (int i = 0; i < 4; i++)
	{
		pzd[i] = ptr[i];
	}
	// Passa valores para o telegrama	
	telegramaMestre[PZD1] = pzd[3];
	telegramaMestre[PZD2] = pzd[2];
	telegramaMestre[PZD3] = pzd[1];
	telegramaMestre[PZD4] = pzd[0];
}
// Trata valor de parâmetros repetidos
// Assim diminui o uso da memória flash do microcontrolador
void trataValorParametroDivisor(uint_fast8_t divisor)
{
	converteParaIEEE754(divisor);
	telegramaMestre [LGE] = 0x0E; 
	telegramaMestre[PZD5] = 0x04;
	telegramaMestre[PZD6] = 0x7E;
	telegramaMestre[PZD7] = 0x00;
	telegramaMestre[PZD8] = 0x00;
}
/********* Envia telegrama: UART0 -> UART1 / CLP -> Inversores **********/
void enviaTelegramaCLP(void)
{
	// OK, pode transmitir
	// Habilita a transmissão no pino do MAX485		
	PORTB |= (1 << PINB4);
	// Espera até estabilizar a tensão no pino
	_delay_loop_1(2);
	// Transmite os 14 bytes na porta serial
	indiceTelegramaCLP = 0;
	do
	{
		// Aguarda até que o buffer esteja vazio
		while (!(UCSR1A & (1 << UDRE1)));
		// Transmite o byte no índice
		UDR1 = telegramaMestre[indiceTelegramaCLP++];
	} while(indiceTelegramaCLP < comprimentoTelegramaCLP);
	// Limpa o buffer do telegrama
	for (indiceTelegramaCLP = comprimentoTelegramaCLP; indiceTelegramaCLP != 0; indiceTelegramaCLP--)
	{
		telegramaMestre[indiceTelegramaCLP] = 0x00;
	}
	// Espera até que os 2 últimos bytes tenham sido transmitidos
	// Em 9600bps, 1 byte = 1146us
	_delay_us(2292);	
	// Desabilita a transmissão no pino do MAX485
	PORTB &= ~(1 << PINB4);
}
/********* Envia telegrama: UART1 -> UART0 / Inversores -> CLP **********/
void enviaTelegramaInversores(void)
{
	// OK, pode transmitir
	// Habilita a transmissão no pino do MAX485		
	PORTD |= (1 << PIND2);
	// Espera até estabilizar a tensão no pino
	_delay_loop_1(2);
	// Transmite os 14 bytes na porta serial
	indiceTelegramaInversores = 0;
	do
	{
		// Aguarda até que o buffer esteja vazio
		while (!(UCSR0A & (1 << UDRE0)));
		// Transmite o byte no éndice
		UDR0 = telegramaInversores[indiceTelegramaInversores++];
	} while(indiceTelegramaInversores < comprimentoTelegramaInversores);
	// Limpa o buffer do telegrama
	for (indiceTelegramaInversores = comprimentoTelegramaInversores; indiceTelegramaInversores != 0; indiceTelegramaInversores--)
	{
		telegramaInversores[indiceTelegramaInversores] = 0x00;
	}
	// Espera até que os 2 últimos bytes tenham sido transmitidos
	// Em 9600bps, 1 byte = 1146us
	_delay_us(2292);
	// Desabilita a transmissão no pino do MAX485
	PORTD &= ~(1 << PIND2);
}
/********* Monta o telegramaMestre, traduzindo os bytes necessários **********/
void traduzTelegramaCLP(void)
{
	// Buferiza PZD1 e PZD2 para depois repassá-los exatamente ao CLP
	valorPZD1 = telegramaMestre[PZD1];
	valorPZD2 = telegramaMestre[PZD2];
	// Buferiza PKE1 e PKE2 para caso de parâmetro não previsto na tradução
	valorPKE1 = telegramaMestre[PKE1];
	valorPKE2 = telegramaMestre[PKE2];
    // Decodifica o número do parâmetro sem o "nibble" de tarefa do byte MSB
    parametro = (((telegramaMestre[PKE1] << 8) & 0b00001111) | telegramaMestre[PKE2]);
	//
	if (parametro == 930)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 0)
	{
		valorPKE1 = telegramaMestre[PKE1];
		telegramaMestre[PKE1] = 0x00;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 1)
	{
		telegramaMestre[PKE1] = 0x20;
		telegramaMestre[PKE2] = 0x0A;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		telegramaMestre[PZD2] = 0x00;
	}
	else if (parametro == 2)
	{
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x60;
		 trataValorParametroDivisor(10);
	}
	else if (parametro == 3)
	{
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x61;
		 trataValorParametroDivisor(10);
	}
	else if (parametro == 4)
	{
		telegramaMestre[PKE1] = 0x31;
		telegramaMestre[PKE2] = 0x05;
		telegramaMestre[PKE3] = 0x80;
		trataValorParametroDivisor(10);
	}
	else if (parametro == 5)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 6)
	{
		 telegramaMestre[PKE1] = 0x73;
		 telegramaMestre[PKE2] = 0xE8;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 switch (telegramaMestre[PZD2])
		 {
			 case 0 :
			 {
				 telegramaMestre[PZD2] = 1;
				 break;
			 }
			 case 1 :
			 {
				 telegramaMestre[PZD2] = 2;
				 break;
			 }
			 case 2 :
			 {
				 telegramaMestre[PZD2] = 3;
				 break;
			 }
		 }		 
	}
	else if (parametro == 7)
	{
		 telegramaMestre[PKE1] = 0x72;
		 telegramaMestre[PKE2] = 0xBC;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 switch (telegramaMestre[PZD2])
		 {
			case 0 :
			{
				telegramaMestre[PZD2] = 2;
				break;
			}
		 }
	}
	else if (parametro == 9)
	{
		 telegramaMestre[PKE1] = 0x20;
		 telegramaMestre[PKE2] = 0x04;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 switch (telegramaMestre[PZD2])
		 {
			case 3 :
			{
				telegramaMestre[PZD2] = 0;
				break;
			}
		 }
	}
	else if (parametro == 11)
	{
		 telegramaMestre[PKE1] = 0x74;
		 telegramaMestre[PKE2] = 0x07;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 12)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x10;
		 telegramaMestre[PZD1] = 0x00;
		 telegramaMestre[PZD2] = 0x00;
		 telegramaMestre[PZD3] = 0x00;
		 telegramaMestre[PZD4] = 0x00;
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 13)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x3A;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 14)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x43;
		 converteParaIEEE754(100);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 15)
	{
		 telegramaMestre[PKE1] = 0x24;
		 telegramaMestre[PKE2] = 0xBA;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 switch (telegramaMestre[PZD2])
		 {
			 case 1 :
			 {
				 telegramaMestre[PZD2] = 2;
				 break;
			 }
			 default:
			 {
				 telegramaMestre[PZD2] = 0;
				 break;
			 }
		 }
	}
	else if (parametro == 16)
	{
		telegramaMestre[PKE1] = 0x24;
		telegramaMestre[PKE2] = 0xB0;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		switch (telegramaMestre[PZD2])
		{
			case 1 :
			{
				telegramaMestre[PZD2] = 2;
				break;
			}
			case 2 :
			{
				telegramaMestre[PZD2] = 1;
				break;
			}
			case 3 :
			{
				telegramaMestre[PZD2] = 5;
				break;
			}
			case 4 :
			{
				telegramaMestre[PZD2] = 4;
				break;
			}
			default:
			{
				telegramaMestre[PZD2] = 0;
				break;
			}
		}
	}
	else if (parametro == 17)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 18)
	{
		 telegramaMestre[PKE1] = 0x24;
		 telegramaMestre[PKE2] = 0xBB;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 if (telegramaMestre[PZD2] == 0x01)
		 {
			telegramaMestre[PZD2] = 5;
		 }
	}
	else if (parametro == 21)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 22)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 23)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 24)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 25)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 31)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 32)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 33)
	{
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x24;
		 trataValorParametroDivisor(10);
	}
	else if (parametro == 34)
	{
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x25;
		 trataValorParametroDivisor(10);
	}
	else if (parametro == 41)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xE9;
		 if (telegramaMestre[ADR] == 0x01)
		 {
		 	telegramaMestre[PZD1] = 0x02;
		 	telegramaMestre[PZD2] = 0x06;
		 }
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 42)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xEA;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 43)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xEB;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 44)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xEC;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 45)
	{
		telegramaMestre[PKE1] = 0x22;
		telegramaMestre[PKE2] = 0xEC;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 46)
	{
		telegramaMestre [LGE] = 0x0E;
		telegramaMestre[PKE1] = 0x84;
		telegramaMestre[PKE2] = 0x38;
		converteParaIEEE754(10);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 47)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xEE;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 48)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xEF;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 49)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x83;
		 telegramaMestre[PKE2] = 0xF0;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 50)
	{
		telegramaMestre[PKE1] = 0x22;
		telegramaMestre[PKE2] = 0xEC;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 51)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0xBD;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 52)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0xBE;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 53)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0xBF;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		if (telegramaMestre[PZD2] == 0x11)
		{
			telegramaMestre[PZD2] = 0x0F;
		}		
	}
	else if (parametro == 54)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0xC0;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 55)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0xBE;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 56)
	{
		telegramaMestre[PKE1] = 0x22;
		telegramaMestre[PKE2] = 0xD4;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		switch (telegramaMestre[PZD2])
		{
			case 0 :
			{
				telegramaMestre[PZD2] = 3;
				break;
			}
			case 1 :
			{
				telegramaMestre[PZD2] = 2;
				break;
			}
			case 2 :
			{
				telegramaMestre[PZD2] = 1;
				break;
			}
		}
	}
	else if (parametro == 61)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x82;
		telegramaMestre[PKE2] = 0xDC;
		switch (telegramaMestre[PZD2])
		{
			case 1 :
			{
				telegramaMestre[PZD2] = 52;
				telegramaMestre[PZD4] =  2;
				break;
			}
			case 6 :
			{
				telegramaMestre[PZD2] = 52;
				telegramaMestre[PZD4] =  3;
				break;
			}
			default:
			{
				telegramaMestre[PZD2] =  2;
				break;
			}
		}
		telegramaMestre[PZD3] = 0x00;
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 62)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x82;
		telegramaMestre[PKE2] = 0xDB;
		switch (telegramaMestre[PZD2])
		{
			case 1 :
			{
				telegramaMestre[PZD2] = 52;
				telegramaMestre[PZD4] =  2;
				break;
			}
			case 6 :
			{
				telegramaMestre[PZD2] = 52;
				telegramaMestre[PZD4] =  3;
				break;
			}
			default:
			{
				telegramaMestre[PZD2] =  3;
				break;
			}
		}
		telegramaMestre[PZD3] = 0x00;
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 63)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 64)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 65)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 70)
	{
		 telegramaMestre[PKE1] = 0x63;
		 telegramaMestre[PKE2] = 0xB3;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 71)
	{
		telegramaMestre [LGE] = 0x0E;
		telegramaMestre[PKE1] = 0x85;
		telegramaMestre[PKE2] = 0x37;
		converteParaIEEE754(1);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 73)
	{
		telegramaMestre[PKE1] = 0x74;
		telegramaMestre[PKE2] = 0xD0;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 74)
	{
		telegramaMestre[PKE1] = 0x72;
		telegramaMestre[PKE2] = 0x62;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		switch (telegramaMestre[PZD2])
		{
			case 0 :
			{
				telegramaMestre[PZD2] = 5;
				break;
			}
			case 4 :
			{
				telegramaMestre[PZD2] = 6;
				break;
			}
			default:
			{
				telegramaMestre[PZD2] = 5;
				break;
			}
		}
	}
	else if (parametro == 75)
	{
		telegramaMestre[PKE1] = 0x24;
		telegramaMestre[PKE2] = 0xD5;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 76)

	{
		telegramaMestre[PKE1] = 0x27;
		telegramaMestre[PKE2] = 0x08;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		switch (telegramaMestre[PZD2])
		{
			case 0 :
			{
				telegramaMestre[PZD2] = 16;
				break;
			}
			case 1 :
			{
				telegramaMestre[PZD2] = 16;
				break;
			}
			case 2 :
			{
				telegramaMestre[PZD2] = 8;
				break;
			}
			case 3 :
			{
				telegramaMestre[PZD2] = 8;
				break;
			}
			case 6 :
			{
				telegramaMestre[PZD2] = 2;
				break;
			}
			case 7 :
			{
				telegramaMestre[PZD2] = 2;
				break;
			}
			default:
			{
				telegramaMestre[PZD2] = 8;
				break;
			}
		}
	}
	else if (parametro == 77)
	{
		telegramaMestre[PKE1] = 0x75;
		telegramaMestre[PKE2] = 0x14;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 78)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x85;
		telegramaMestre[PKE2] = 0x1F;
		converteParaIEEE754(1);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 79)
	{
		telegramaMestre[PKE1] = 0x20;
		telegramaMestre[PKE2] = 0x0A;
		telegramaMestre[PZD1] = 0x00;
		telegramaMestre[PZD2] = 0x01;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		telegramaMestre[PZD5] = 0x00;
		telegramaMestre[PZD6] = 0x00;
	}
	else if (parametro == 81)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x81;
		 telegramaMestre[PKE2] = 0x36;
		 converteParaIEEE754(10);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 82)
	{
		 telegramaMestre[PKE1] = 0x71;
		 telegramaMestre[PKE2] = 0x37;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 83)
	{
		 telegramaMestre[PKE1] = 0x81;
		 telegramaMestre[PKE2] = 0x31;
		 trataValorParametroDivisor(10);
	}
	else if (parametro == 84)
	{
		 telegramaMestre[PKE1] = 0x71;
		 telegramaMestre[PKE2] = 0x30;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
	}
	else if (parametro == 85)
	{
		 telegramaMestre [LGE] = 0x0E; 
		 telegramaMestre[PKE1] = 0x81;
		 telegramaMestre[PKE2] = 0x33;
		 converteParaIEEE754(100);
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
		 parametro = 85;
	}
	else if (parametro == 86)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x82;
		telegramaMestre[PKE2] = 0x80;
		converteParaIEEE754(1);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 87)
	{
		telegramaMestre[PKE1] = 0x20;
		telegramaMestre[PKE2] = 0x0A;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		telegramaMestre[PZD2] = 0x00;
	}
	else if (parametro == 88)
	{
		telegramaMestre[PKE1] = 0x71;
		telegramaMestre[PKE2] = 0x54;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		if (telegramaMestre[PZD2] == 1)
		{
			telegramaMestre[PZD2] = 2;
		}
	}
	else if (parametro == 89)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x81;
		telegramaMestre[PKE2] = 0x5E;
		converteParaIEEE754(100);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 95)
	{
		telegramaMestre[PKE1] = 0x20;
		telegramaMestre[PKE2] = 0x0A;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		telegramaMestre[PZD2] = 0x01;
	}
	else if (parametro == 101)
	{
		 telegramaMestre[PKE1] = 0x20;
		 telegramaMestre[PKE2] = 0x64;
		 telegramaMestre[PZD3] = 0x04;
		 telegramaMestre[PZD4] = 0x7E;
		 if (telegramaMestre[PZD2] == 1)
		 {
			telegramaMestre[PZD2] = 2;
		 }
	}
	else if (parametro == 121)
	{
		telegramaMestre[PKE1] = 0x20;
		telegramaMestre[PKE2] = 0x0A;
		telegramaMestre[PZD3] = 0x04;
		telegramaMestre[PZD4] = 0x7E;
		telegramaMestre[PZD2] = 0x00;
	}
	else if (parametro == 122)
	{
		 telegramaMestre [LGE] = 0x0E; 		 
		 telegramaMestre[PKE1] = 0x84;
		 telegramaMestre[PKE2] = 0x0B;
		 if (telegramaMestre[PZD2] > 0x00)
		 {
			 telegramaMestre[PZD2] = 19;
			 telegramaMestre[PZD4] = 14;
		 }
		 else
		 {
			 telegramaMestre[PZD2] = 0x00;
			 telegramaMestre[PZD4] = 0x00;
		 }	 
		 telegramaMestre[PZD3] = 0x00;
		 telegramaMestre[PZD5] = 0x04;
		 telegramaMestre[PZD6] = 0x7E;
		 telegramaMestre[PZD7] = 0x00;
		 telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 123)
	{
		telegramaMestre [LGE] = 0x0E;
		telegramaMestre[PKE1] = 0x85;
		telegramaMestre[PKE2] = 0x20;
		telegramaMestre[PZD1] = 0x00;
		telegramaMestre[PZD2] = 0x14;
		converteParaIEEE754(1);
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	else if (parametro == 124)
	{
		telegramaMestre [LGE] = 0x0E; 
		telegramaMestre[PKE1] = 0x84;
		telegramaMestre[PKE2] = 0x0C;
		if (telegramaMestre[PZD2] > 0x00)
		{
			telegramaMestre[PZD2] = 19;
			telegramaMestre[PZD4] = 14;
		}
		else
		{
			telegramaMestre[PZD2] = 0x00;
			telegramaMestre[PZD4] = 0x00;
		}
		telegramaMestre[PZD3] = 0x00;
		telegramaMestre[PZD5] = 0x04;
		telegramaMestre[PZD6] = 0x7E;
		telegramaMestre[PZD7] = 0x00;
		telegramaMestre[PZD8] = 0x00;
	}
	// Atualiza o comprimento do telegrama
	comprimentoTelegramaCLP = (telegramaMestre[LGE] + 2); // USS pág. 9, LGE = LGE + STX + BCC
	// Zera o BCC no telegrama para o cálculo
	telegramaMestre[BCCM] = 0x00;
	for (indiceTelegramaCLP = 0; indiceTelegramaCLP < (comprimentoTelegramaCLP - 1); indiceTelegramaCLP++)
	{
		telegramaMestre[BCCM] ^= telegramaMestre[indiceTelegramaCLP];
	}
}
/********* Monta o telegramaInversores, traduzindo os bytes necess?rios **********/
void traduzTelegramaInversores(void)
{
	if (parametro == 930)
	{
		telegramaInversores[PKE1] = 0x13;
		telegramaInversores[PKE2] = 0xA2;
		telegramaInversores[PZD1] = 0x00;
		switch (telegramaInversores[PZD2])
		{
			case 1:
			{
				telegramaInversores[PZD2] = 0; // Sem falha
				break;
			}
			case 2:
			{
				telegramaInversores[PZD2] = 1; // F001 Overvoltage
				break;
			}
			case 3:
			{
				telegramaInversores[PZD2] = 2; // F002 Overcurrent
				break;
			}
			case 4:
			{
				telegramaInversores[PZD2] = 3; // F003 Overload
				break;
			}
			case 5:
			{
				telegramaInversores[PZD2] = 4; // F004 Overtemperature, motor
				break;
			}
			case 6:
			{
				telegramaInversores[PZD2] = 101; // F101 Internal interface fault
				break;
			}
			case 60:
			{
				telegramaInversores[PZD2] = 11; // F011 Fault, internal interface
				break;
			}
			case 71:
			{
				telegramaInversores[PZD2] = 8; // F008 USS protocol time out
				break;
			}
			case 72:
			{
				telegramaInversores[PZD2] = 33; // F033 DP bus configuration fault
				break;
			}
			case 85:
			{
				telegramaInversores[PZD2] = 12; // F012 External trip
				break;
			}
			case 100:
			{
				telegramaInversores[PZD2] = 10; // F010 Initialization fault
				break;
			}
			case 101:
			{
				telegramaInversores[PZD2] = 13; // F013 Program fault
				break;
			}
			case 105:
			{
				telegramaInversores[PZD2] = 12; // F12 Inverter temperature signal lost
				break;
			}
		}
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 0)
	{
		 telegramaInversores[PKE1] = valorPKE1;
		 telegramaInversores[PKE2] = 0x00;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;		 
		 if (telegramaInversores[ADR] == 1)
		 {
			telegramaInversores[PZD3] = 0x0B; 
		 }
		 else
		 {
			telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 1)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x01;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 2)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x02;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 3)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x03;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 4)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x04;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 5)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x05;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 6)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x06;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 7)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x07;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}	
	else if (parametro == 9)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x09;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}	
	else if (parametro == 11)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x0B;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 12)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x0C;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 13)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x0D;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 14)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x0E;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 15)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x0F;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 16)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x10;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 17)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x11;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 18)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x12;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 21)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x15;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 22)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x16;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 23)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x17;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 24)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x18;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 25)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x19;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 31)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x1F;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 32)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x20;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 33)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x21;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 34)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x22;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 41)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x29;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 42)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2A;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 43)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2B;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 44)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2C;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 45)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2D;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 46)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2E;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 47)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x2F;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 48)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x30;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 49)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x31;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 50)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x32;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 51)
	{
		 telegramaInversores[PKE1] = 0x10;
		 telegramaInversores[PKE2] = 0x33;
		 telegramaInversores[PKE3] = 0x00;
		 telegramaInversores[PKE4] = 0x00;
		 telegramaInversores[PZD5] = 0x00;
		 telegramaInversores[PZD6] = 0x00;
		 if (telegramaInversores[ADR] == 1)
		 {
			 telegramaInversores[PZD3] = 0x0B;
		 }
		 else
		 {
			 telegramaInversores[PZD3] = 0x13;
		 }
		 telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 52)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x34;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 53)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x35;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 54)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x36;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 55)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x37;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 56)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x38;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 61)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x3D;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 62)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x3E;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 71)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x47;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 63)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x3F;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 64)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x40;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 65)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x41;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 70)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x46;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 71)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x47;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 72)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x48;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 73)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x49;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 74)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4A;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 75)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4B;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 76)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4C;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 77)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4D;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 78)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4E;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 79)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x4F;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 81)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x51;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 82)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x52;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 83)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x53;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 84)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x54;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 85)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x55;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 86)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x56;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 87)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x57;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 88)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x58;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 89)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x59;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 93)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x5D;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 95)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x5F;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}	
	else if (parametro == 101)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x65;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 121)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x79;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 122)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x7A;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 123)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x7B;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	else if (parametro == 124)
	{
		telegramaInversores[PKE1] = 0x10;
		telegramaInversores[PKE2] = 0x7C;
		telegramaInversores[PKE3] = 0x00;
		telegramaInversores[PKE4] = 0x00;
		telegramaInversores[PZD5] = 0x00;
		telegramaInversores[PZD6] = 0x00;
		if (telegramaInversores[ADR] == 1)
		{
			telegramaInversores[PZD3] = 0x0B;
		}
		else
		{
			telegramaInversores[PZD3] = 0x13;
		}
		telegramaInversores[PZD4] = 0x72;
	}
	// sem else
	if (parametro != 930)
	{
		telegramaInversores[PZD1] = valorPZD1;
		telegramaInversores[PZD2] = valorPZD2;
	}
	// Padrão para todo telegrama vindo dos inversores
	telegramaInversores [LGE] = 0x0C; // Comprimento do inversor legado
	comprimentoTelegramaInversores = (telegramaInversores[LGE] + 2); // USS pág. 9, LGE = LGE + STX + BCC
	// Zera o BCC no telegrama para o cálculo
	telegramaInversores[BCCE] = 0x00;
	for (indiceTelegramaInversores = 0; indiceTelegramaInversores < (comprimentoTelegramaInversores - 1); indiceTelegramaInversores++)
	{
		telegramaInversores[BCCE] ^= telegramaInversores[indiceTelegramaInversores];
	}
}
/*************************** Interrupções *******************************/
ISR(USART0_RXC_vect)
{
	// Desabilita as interrupções
	cli();
	// Captura o dado recebido para o telegrama e incrementa o índice
	do 
	{
		// Aguarda até que haja dado no buffer
		while (!(UCSR0A & (1 << RXC0)));
		// OK, já há dado, guarde-o no telegrama
		telegramaMestre[indiceTelegramaCLP] = UDR0;
		// Captura o comprimento do telegrama
		if (telegramaMestre[LGE] != 0x00) // O comprimento não pode ser nulo
		{
			comprimentoTelegramaCLP = (telegramaMestre[LGE] + 2); // USS pág. 9, LGE = LGE + STX + BCC
		}
		else // Por algum motivo, o byte da posição 1 contém zero, então façaa-o igual a 14
		{
			comprimentoTelegramaCLP = 14; // 14 é o valor padrão do inversor legado
		}		
		// Verifica se é o início do telegrama
		if (telegramaMestre[STX] != 0x02)
		{
			// Não é! Zera o índice para que se possa recomeçar a contagem
			indiceTelegramaCLP = 0;
			// Reabilita as interrupções
			sei();
			// Retorna ao stack pointer e continua		
			return;
		}
	} while ((indiceTelegramaCLP++) < (comprimentoTelegramaCLP - 1)); // Faz até que o telegrama esteja completo
	// OK, pode reabilitar as interrupções, o telegrama será transmitido em main
	sei();
}
/************************************************************************/
ISR(USART1_RXC_vect)
{
	// Desabilita as interrupções
	cli();
	// Captura o dado recebido para o telegrama e incrementa o índice
	do
	{
		// Aguarda até que haja dado no buffer
		while (!(UCSR1A & (1 << RXC1)));
		// OK, já há dado, guarde-o no telegrama
		telegramaInversores[indiceTelegramaInversores] = UDR1;
		// Captura o comprimento do telegrama
		if (telegramaInversores[LGE] != 0x00) // O comprimento não pode ser nulo
		{
			comprimentoTelegramaInversores = (telegramaInversores[LGE] + 2); // USS pág. 9, LGE = LGE + STX + BCC
		}
		else // Por algum motivo, o byte da posição 1 contém zero, então faça-o igual a 14
		{
			comprimentoTelegramaInversores = 14; // 14 é o valor padrão do inversor legado
		}
		// Verifica se é o início do telegrama
		if (telegramaInversores[STX] != 0x02)
		{
			// Não é! Zera o índice para que se possa recomeçar a contagem
			indiceTelegramaInversores = 0;
			// Reabilita as interrupções			
			sei();
			// Retorna ao stack pointer e continua
			return;
		}
	} while ((indiceTelegramaInversores++) < (comprimentoTelegramaInversores - 1)); // Faz até que o telegrama esteja completo
	// OK, pode reabilitar as interrupções, o telegrama seré transmitido em main
	sei();
}
/******************************** FIM ***********************************/
