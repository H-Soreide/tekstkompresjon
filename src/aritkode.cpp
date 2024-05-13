/*
	aritkode.cpp
	Bibliotek som gjør aritmetisk koding
*/
#include <assert.h>
#include <stdlib.h>
#include "aritkode.hpp"
/*
	Bruker 128 bit til å regne med. Se http://www.drdobbs.com/cpp/data-compression-with-arithmetic-encodin/240169251?pgno=3
  Kan dermed bruke 65 bit på kodeverdi, og 63 bit på frekvens. (Om vi ønsker maksimalt med bits for frekvens) 
  Men, kommer aldri til å kode en så stor fil!
  16M blokk: trenger maksimalt 24 bit for frekvens
  4GB blokk: trenger maskimalt 32 bit for frekvens
  
	Satser på max 32 bit for frekvens, og resterende 96 bits for kode
*/

/*
//Alloker frekvensmodell for et antall symboler.
//Hvis "mer", legg til rette for et nivå til
modell *mod_ny(int maxtegn, int mer) {
	modell *m = calloc(1, sizeof(modell));
	if (!m) feil("For lite minne for frekvensmodell (1)");
	m->maxtegn = maxtegn;
	m->frekvens = calloc(maxtegn, sizeof(int));
	m->start = calloc(maxtegn+1, sizeof(int));
	m->tid = calloc(maxtegn, sizeof(int));
	if (mer) {
		m->neste = calloc(maxtegn, sizeof(modell *));
		m->ix = calloc(maxtegn, sizeof(int));
	}
	if (!m->frekvens || !m->start || !m->tid || (mer && (!m->neste || !m->ix))) feil("For lite minne for frekvensmodell (2)");	
	m->tidsteller = 1;
	return m;
}

//Akkumuler frekvenser fra et gitt symbol og opp
void mod_akkumuler(const modell *m, const int min) {
//	for (int i = min+1; i <= m->maxtegn; ++i) m->start[i] = m->start[i-1] + m->frekvens[i-1];
	m->start[m->maxtegn] = 0; //!!!Bruk heller m->k_total ?
	for (int i=0; i < m->maxtegn; ++i) m->start[m->maxtegn] += m->frekvens[i];
}

void mod_fjern(const modell *m, const int tegn) {
return;//!!!
	--m->frekvens[tegn];
	mod_akkumuler(m, tegn);
}

//Finn og sett maksfrekvens og neststørste frekvens
void mod_finn_maxf(modell * const m) {
	m->maxfrekvens = m->maxf2 = 0;
	for (int i = 0; i < m->maxtegn; ++i) if (m->frekvens[i] > m->maxfrekvens) {
		//Fant nytt maksimum
		m->maxf2 = m->maxfrekvens;
		m->maxfrekvens = m->frekvens[i];
	} else if (m->frekvens[i] > m->maxf2) { //ikke ny max, kan være ny maxf2
		m->maxf2 = m->frekvens[i];
	}
}
*/
#define MAXINT32 0xFFFFFFFF
#define MAXINT64 0xFFFFFFFFFFFFFFFF
#define MAXINT96 ((((__int128) MAXINT32) << 64) | MAXINT64)
#define MAXINT95 (MAXINT96 >> 1)
#define BIT95 (((__int128) 1) << 95)
#define BIT94 (((__int128) 1) << 94)
#define HALV BIT95
#define KVART BIT94
#define TREKVART (BIT95 | BIT94)

//Konstruktør
aritkode::aritkode(FILE *pfil) {
	//Aritmetisk koding
	lo = 0;
	hi = MAXINT96;
	verdi = 0;
	ekstrabits = 0;
	//io-buffer
	filbyte = 0;
	filbits = 0;
	fil = pfil;
}

//Skriv 1 bit til filbuffer/fil
void aritkode::output_bits(char const bit) {
	if (filbits == 8) {
		putc(filbyte, fil);
		filbits = 0;
		filbyte = 0;
	}
	filbyte = (filbyte << 1) | bit;
	++filbits;
	while (ekstrabits) {
		--ekstrabits;
		if (filbits == 8) {
			putc(filbyte, fil);
			filbits = 0;
			filbyte = 0;
		}
		filbyte = (filbyte << 1) | !bit;
		++filbits;
	}
}

//Koder ett intervall med aritmetisk koding	
//Lavnivåfunksjon som gjør jobben
//0..p_cnt er HELE det mulige intervallet (sannsynlighet 1)
//p_lo..p_hi er intervallet som skal kodes.
//0 <= p_lo < p_hi <= p_cnt
//Hvis p_lo == 0 og p_hi == p_cnt, brukes ingen bits
//Jo større del intervallet er av helheten, jo færre bits går med.
//Brukes av andre kod_ metoder
//Brukes også direkte, av statistiske metoder
void	aritkode::kod_intervall(unsigned int const p_lo, unsigned int const p_hi, unsigned int const p_cnt) {
	unsigned __int128 const intervall = hi - lo + 1;
	hi = lo + (intervall * p_hi) / p_cnt - 1;
	lo = lo + (intervall * p_lo) / p_cnt;

	for (;;) {

		if (hi < HALV) {
			output_bits(0);
		} else if (lo >= HALV) {
			output_bits(1);
			lo &= MAXINT95; //Masker vekk øverste bit fordi den er satt
			hi &= MAXINT95;
		} else if (lo >= KVART && hi < TREKVART ) {
			++ekstrabits;
			lo -= KVART;
			hi -= KVART; //Er nå mindre enn HALV
		} else break;
		hi = (hi << 1) | 1;
		lo <<= 1;
	}
}


//Koder ett symbol med aritmetisk koding. (Legger til et intervall)
//Høynivåfunksjon som koder et tegn, ved hjelp av en statistisk modell (frekvenstabell)
/*
void kod(arit * const a, modell *const m, const unsigned char c) {
	int f_ix = m->ix[c];
	assert(f_ix <= m->k_siste);
	if (f_ix > m->k_forrige) { //Akkumuler opp hit
		for (int i = m->k_forrige; i <= f_ix; ++i) m->start[i+1] = m->start[i] + m->frekvens[i];
	}
	const unsigned int p_lo = m->start[f_ix];
	const unsigned int p_hi = m->start[f_ix + 1];
	const unsigned int p_cnt = m->k_total;

	kod_intervall(a, p_lo, p_hi, p_cnt);
	//Reduser statistikken:
	--m->k_total;
	--m->frekvens[f_ix];
	--m->start[f_ix + 1];
	//Sjekk om vi åpner et nytt symbol
	if (f_ix == m->k_siste && m->k_siste < m->ant_ix) {
		++m->k_siste;
		m->k_total += m->frekvens[m->k_siste];
	}
	m->k_forrige = f_ix;
}
*/

//Koder et heltall. min <= tall <= max. Alle tallverdier like sannsynlige
void aritkode::kod_tall(int const tall, int const min, int const max) {
	assert(min <= tall && tall <= max);
	kod_intervall(tall-min, tall-min+1, (max-min)+1);
}

//Koder en sjanse (true/false). Optimalt hvis sjansen er x av P for true.
void aritkode::kod_sjanse(bool const sjanse, unsigned int const x, unsigned int const P) {
	if (!P) return; //No-op for 0 sannsynlighet
	assert(0 < x && x < P);
	if (sjanse) kod_intervall(P-x, P, P);
	else kod_intervall(0, P-x, P);
}


//Får ut to bits som trengs for å dekode slutten, og evt. ekstrabits.
//Runder deretter output opp til nærmeste hele byte
void aritkode::kod_avslutt() {
	++ekstrabits;
	output_bits((lo & TREKVART) ? 1 : 0);
	//Runde opp
	if (!filbits) return; //Neppe mulig
	filbyte <<= 8-filbits;
	putc(filbyte, fil);
}


//Må kunne hente opptil 96 ekstra bits (12 byte) i fall vi har en kortere fil enn det.
//filbits er masken for neste bit, eller 0 hvis vi må hente en byte
unsigned int aritkode::input_bit() {
	if (!filbits) {
		int x = getc(fil);
		if (x == EOF) {
			filbyte = 0;
			ekstrabits += 8;
			if (ekstrabits > 96) {
				printf("Dekompresjonsfeil");
				exit(1);
			}
		} else filbyte = x;
		filbits = 128;
	}
	unsigned int retbit = (filbyte & filbits) ? 1 : 0;
	filbits >>= 1;
	return retbit; 
}

//Gjør klart ved å lese inn de første 96 bits
void aritkode::dekod_forbered() {
	for (int i = 96; i--; ) verdi = (verdi << 1) | input_bit();
}

//Lavnivåfunksjon som dekoder ett tegn, ved å lese diverse bits
//strengt tatt er dekodingen gjort, men tilstanden oppdateres 
//matematisk så videre dekoding virker.
void aritkode::dekod_intervall(unsigned __int128 const intervall, unsigned const int p_lo, unsigned const int p_hi, unsigned const int p_cnt) {

	hi = lo + (intervall * p_hi) / p_cnt - 1;
	lo = lo + (intervall * p_lo) / p_cnt;

	for (;;) {
		if (hi < HALV) ;       //0-bit
		else if (lo >= HALV) { //1-bit
			lo &= MAXINT95;      //Null ut biten som skiftes oppover
			hi &= MAXINT95;
			verdi &= MAXINT95;
		} else if (lo >= KVART && hi < TREKVART) {
			lo -= KVART;
			hi -= KVART;
			verdi -= KVART;
		} else break;
		lo <<= 1;
		hi = (hi << 1) | 1;
		verdi = (verdi << 1) | input_bit();
	}
}

/*
//Dekod et tegn, ved hjelp av statistisk modell (frekvenstabell)
unsigned char dekod(arit * const a, modell const *m) {
	const unsigned int p_cnt = m->start[m->maxtegn];
	const unsigned __int128 intervall = a->hi - a->lo + 1;
	const unsigned int cnt = ((a->verdi - a->lo + 1) * p_cnt - 1) / intervall;
	const unsigned char tegn = mod_lookup(cnt, m);
	dekod_intervall(a, m->start[tegn], m->start[tegn+1], p_cnt);
	return tegn;
}
*/
int aritkode::dekod_tall(int const min, int const max) {
	const unsigned int p_cnt = max - min + 1;
  const unsigned __int128 intervall = hent_intervall();
  const unsigned int cnt = hent_cnt(p_cnt, intervall);
	//Tallet er cnt+min. p_lo er cnt, p_hi = cnt+1
	//trenger "resten av dekod"
	dekod_intervall(intervall, cnt, cnt+1, p_cnt);
	return cnt + min;
}

//Dekoder en sjanse (true/false). Optimalt hvis sjansen er x mot P for true.
bool aritkode::dekod_sjanse(unsigned int const x, unsigned int const P) {
	if (!P) return false; //no-op, hvis det ikke er noen sjanse
	const unsigned __int128 intervall = hent_intervall();
	const unsigned int cnt = hent_cnt(P, intervall);
	bool ret = (cnt == P-1);
	if (ret) dekod_intervall(intervall, P-1, P, P);
	else dekod_intervall(intervall, 0, P-1, P);
	return ret;
}

