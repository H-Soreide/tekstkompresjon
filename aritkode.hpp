/*
	aritkode.hpp
	Bibliotek som gjør aritmetisk koding
*/
#ifndef _aritkode
#define _aritkode

#include <stdio.h>


//Alt som trengs for en aritmetisk koder/dekoder
class aritkode {
	private:
	//For aritmetisk koding og dekoding
	unsigned __int128 hi, lo, verdi;
  int ekstrabits;
	//Byte-buffer for fil
	unsigned int filbits;
	unsigned char filbyte;
	FILE *fil;

	void output_bits(char const bit);
	unsigned int input_bit();

	public:
	//Konstruktør
  aritkode(FILE *const fil);

	//Koding
	void kod_intervall(unsigned int const p_lo, unsigned int const p_hi, unsigned int const p_cnt);
	void kod_tall(int const tall, int const min, int const max);
	void kod_sjanse(bool const sjanse, unsigned int const x, unsigned int const P);
	void kod_avslutt();
	//Dekoding
	void dekod_forbered();
	void dekod_intervall(unsigned __int128 const intervall, unsigned const int p_lo, unsigned const int p_hi, unsigned const int p_cnt);
	inline unsigned __int128 hent_intervall() {return hi - lo + 1;}
	inline unsigned int hent_cnt(unsigned int p_cnt, __int128 intervall) {return ((verdi - lo + 1) * p_cnt - 1) / intervall;}
	int dekod_tall(int const min, int const max);
	bool dekod_sjanse(unsigned int const x, unsigned int const P); 
};
/*
//Et sett frekvenser m.m. som beskriver data vi skal komprimere
typedef struct _modell {
	int maxfrekvens;//Maks-frekvens i dette settet, nyttes for koding av frekvenstabellen
	int maxf2;      //nest-mest frekvensen, også nyttig i praksis
	int maxtegn;    //Antall tegn i denne modellen. Typisk 256, eller mer hvis ekstrasymboler
	int *frekvens;  //frekvens[maxtegn]
	int *start;     //start[maxtegn+1] Akkumulerte frekvenser, start[maxtegn] er totalt antall tegn
	unsigned int *tid; 	//tidsteller for første frekvens
	int tidsteller; //telleverk for tider
	int k_siste;    //Siste åpne frekvens ved koding
	int k_total;    //total sum av frekvenser
	int k_forrige;  //forrige 
	struct _modell **neste;		//Array med neste nivå modeller
	int ant_ix; //Antall frekvenser som ikke er 0
	int *ix; //Array med indexer, sorteringsrekkefølge for neste nivå statistikk
} modell;
*/

#endif

