#define U_CHARSET_IS_UTF8 1

#include <stdio.h>
#include <unicode/brkiter.h>
#include <unicode/ucnv.h>
#include <sys/stat.h>
#include <cassert>
// HS kun til bachelor beregning, ikke i endelig program:
#include <cmath>  // HS

#include "tekstkompressor.hpp"
using namespace icu;

/*

Test icu
http://userguide.icu-project.org/boundaryanalysis#TOC-BreakIterator-Boundary-Analysis-Examples
http://icu-project.org/apiref/icu4c/classicu_1_1BreakIterator.html
http://userguide.icu-project.org/boundaryanalysis

	 */

/*

metoder: 

Alle returnerer dessuten posisjonen i strengen
int first()  sett første pos
int last() sett ETTER siste pos i streng
int previous() sett forrige pos
int next() sett neste pos

following(int)  neste grensepos etter oppgitt strengposisjon
preceding(int)  forrige grensepos foran oppgit pos
Typisk nok er det ikke mulig å sette en bestemt pos? Eller vil following() gi samme pos HVIS det er en grensepos? må testes.

int current() tallet for nåværende pos
isBoundary(int)  er posisjonen en grensepos?


int = .getRuleStatus()  Hva slags break har vi?
UBRK_WORD_NONE   indicates that the boundary does not start a word or number. (spaces,punctuation)

enum UBreakIteratorType
UBRK_CHARACTER Character breaks.
UBRK_WORD Word breaks. 
UBRK_LINE Line breaks.
UBRK_SENTENCE Sentence breaks. 

enum USentenceBreakTag    getRuleStatus()   sentence breaks
UBRK_SENTENCE_TERM  noe som slutter setn. (.?!)
UBRK_SENTENCE_SEP   setn. u. punktum, avsluttet av CR/LF/PS/end of input

enum UWordBreak  getRuleStatus word breaks
UBRK_WORD_NONE Tag value for "words" that do not fit into any of other categories. spaces, punctuation
UBRK_WORD_NUMBER Tag value for words that appear to be numbers
UBRK_WORD_LETTER Tag value for words that contain letters, excluding hiragana, katakana or ideographic characters
UBRK_WORD_KANA Tag value for words containing kana characters
UBRK_WORD_IDEO Tag value for words containing ideographic characters

Muligheter: en iterator går gjennom setningene, en annen ord for ord, en tredje tegn for tegn (for å hente ut enkeltord og tolke dem.)

Evt. plukke ut hele ord med strengoperasjoner

Evt. steppe gjennom alt med ord-iteratoren, og sjekke hvilke posisjoner som dessuten er setningsbrudd.



UText can be used with BreakIterator APIs (character/word/sentence/... segmentation). utext_openUTF8() creates a read-only UText for a UTF-8 string.
(Unngå overhead med 1GB konvertert til UTF-16)

*/


/*
Kompresjon: 
1. les data inn i et buffer (ok)
2. les gjennom buffer med iteratorer, finn antall ord, non-ord og setninger (ok)
3. alloker strukturer
4. les gjennom buffer med iteratorer, lagre unike ord i søketre, lag lister med ordnumre og setn. typer, bygg statistikker
5. kod aritmetisk

	 
 
*/



void feil(char const * const s) {
	printf("%s", s);
	exit(1);
}


/*
	 Globale variabler
	 */
unsigned char *data; //Lager for innfila, deles opp i ord
ord nullord; //Ordlengde 1, inneh. en null-byte;
ord nosep;//Ordlengde 0, «skilletegn» det det ikke er noe.
ord *forrige; //For traversering av ordtrær
int tot_bytes; //Hvor mange byte i innfila
bool dbg=false;
/*
	 Metoder for klassen «ord»
	 */

inline int ord::lengde() {return til-fra+1;}

void ord::ny(int f, int t, int ant) {
	fra = f;
	til = t;
	antall = ant;
	endelse2 = -1;
	start2 = -1;
	fix = 0;
	overlapp = overlapp2 = 0;
	endelse = lengde();

	// HS-------------------
	hyppig = -1;   // HS: Om ordet ikke er blant de hyppigste er hyppig < 0
	//---------------------
}

// HS-------------------
void ord::sett_hyppig(int index) {
	hyppig = index; 	// Setter ordets 'hyppig' til det nummeret det får i listen over ord sortert etter forekomst. Hyppigste ord har hyppig = 0, nest hyppigste = 1 osv.
}
	//---------------------

inline void ord::entil(int ant) { antall += ant; }


ordlager_c::ordlager_c(int kapasitet) {
	this->kapasitet = kapasitet;
	antall = 0;
	ordtabell = new ord[kapasitet];
}

int ordlager_c::nyord(int fra, int til, int ant) {
	if (antall >= kapasitet) {
		printf("Kapasitet: %i\n", kapasitet);
		fflush(stdout);
		feil("For lite plass i ordlager!");
	}
	ordtabell[antall].ny(fra, til, ant);
	return antall++;
}

inline ord *ordlager_c::hent(int ordnr) { return &ordtabell[ordnr]; }
inline int ordlager_c::les_antall() { return antall; }

//enwik8: 1000000 200000 2000
ordlager_c ordlager_b(1800000), ordlager_t(900000), ordlager_n(2000); //Mer enn enwik9 trenger 

/*
	 Metoder for statistikk
	 */

statistikk::statistikk(char const *pnavn, int pmin, int pmax) {
	t = new int[pmax-pmin+1]();
	navn = pnavn;
	tmax = min = pmin; //Ja, dette stemmer...
	tmin = max = pmax;
}

statistikk::~statistikk() {
	delete t;
}


inline void statistikk::reg(int dat) {
	if (dat < min || dat > max) {
		printf("Statistikk %s feil: %i utenfor [%i..%i]\n", navn, dat, min, max);
		feil("AVBRUTT\n");
	}
	++t[dat-min];
	if (dat < tmin) tmin = dat;
	if (dat > tmax) tmax = dat;
}

inline void statistikk::rapport() {
	printf("STATISTIKK for %s\n", navn);
	printf("Min: %9i\n", tmin);
	printf("Max: %9i\n", tmax);
	for (int i = min; i<= max; ++i) {
		printf("%5i: %9i\n", i, t[i-min]);
	}
}

//Annet

void skriv(int fra, int const til) {
	if (fra>til) printf("FEIL");
	printf("«");
	while (fra <= til) putchar(data[fra++]);
	printf("»");
}


// HS ===============================================================================================

void tekstkompressor::hs_stat2() { 

	printf("\n\nStarter på statistikk-kode... \n");

	// ==================  Del 1:   ===============
	// Første del bruker ordfordelingen til å finne de hyppigste ordene
	bokstavordtre->ordfordeling->akkumuler();  // Nødvendig for å sortere og finne max eller har ikke rekkefølge noe å si?
	bokstavordtre->ordfordeling->ixsort();
	bokstavordtre->ordfordeling->finnmax();


	// To cut-off metoder:  
	// 1. basert på frekvens til det aller hyppigste bokstavordet. 
	// 2. basert på et antall hyppigste ord, f.eks 10, 50, 100 hyppigste. 

	// Med metode=2 og cutoff=50 oppretter vi en listestat for de 50 hyppigste ordene og bruker de til å telle frekvens på følger-ord. 
	int metode = 2;
	int cutoff =5;  // Hvis metode 1: ant. vi deler max_f på, hvis metode 2: antall hyppige ord vi oppretter listestat for. 

	int ant_hyppige_listestat = 0; // Denne er viktig!
	// Starter med det aller hyppigste ordet:
	ord *b = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[0]);  
	int sum_hyppigste = 0;


	if (metode ==1) {

		cutoff= bokstavordtre->ordfordeling->max_f / cutoff;   // Delt på 50 er kanskje ok for enwik8. /5 ok på Hound of Baskervilles
		printf("\nCut-off frekv. for hyppigste ord: %i\n", cutoff);
	
		while (b->antall > cutoff) {
			b->sett_hyppig(ant_hyppige_listestat);
			sum_hyppigste += b->antall;
			printf("Ord lagt til hyppigste (Frekv.: %3i, hyppig-nr: %i):  ", b->antall, b->hyppig);
			skriv(b->fra, b->til);
			printf("\n");
			b = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[++ant_hyppige_listestat]);
	}
 
	} else if (metode == 2) {
		printf("\nCut-off antall for hyppigste ord: %i\n", cutoff);

		while (ant_hyppige_listestat < cutoff) {
			b->sett_hyppig(ant_hyppige_listestat);
			sum_hyppigste += b->antall;
			printf("Ord lagt til hyppigste (Frekv.: %3i, hyppig-nr: %i):  ", b->antall, b->hyppig);
			skriv(b->fra, b->til);
			printf("\n");

			b = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[++ant_hyppige_listestat]);

		}
	}

	//printf("Antall hyppigste ord med folger-stat:  %lu\n", sizeof(bokstavordtre->folger_stat)/sizeof(bokstavordtre->folger_stat[0])); //std::end(bokstavordtre->folger_stat) - std::begin(bokstavordtre->folger_stat));

  

	printf("folger_stat opprettes på 'ordtre' med plass til en listestat per hyppige ord:  %i\n", ant_hyppige_listestat);
	bokstavordtre->folger_stat = new listestat*[ant_hyppige_listestat];  

		
	for (int j = 0; j < ant_hyppige_listestat; ++j) {
		bokstavordtre->folger_stat[j] = new listestat(0, bokstavordtre->lesantall());
	} 



	// Tillegg til del 1: HS
	// En cut-off på hvilke ord det er 'verdt' å telle opp. F.eks teller bare opp ord blant de ord som forekommer til sammen 80% av tiden. 
	// Sjeldne ord telles ikke opp, og sannsynligheten deres forblir den globale 0.-ordens. 
	// Videre vil det sannsynligvis være rom for optimering av logikken som teller opp listestat i Del 2 som unngår å sjekke ordenes antall unødig ofte. 
	// Cut-off for ord verdt å telle opp i listestatene.   (Kan nok slås sammen på et vis med logikken over, men adskilt for nå.)

	int akk_hyppige = 0;
	float cut_off_prosent = 0.8f;
	ord *ord_verd_telling = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[0]);   // Starter med å telle hyppigste ord
	int ant = 0;
	int cut_off_total = (int)(bokstavordtre->lestotal() * cut_off_prosent + 0.5f);


	while (akk_hyppige < cut_off_total) {
		akk_hyppige +=  ord_verd_telling->antall;
		ord_verd_telling = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[++ant]);
	}
	int cut_off_verdt_telling = ord_verd_telling->antall;

	printf("\nOrd som utgjør til sammen 80 pct. av tekstens volum har frekvenser på minimum:  %i. Det er %i ord med lik eller høyere frekvens av total %i unike ord.\n", cut_off_verdt_telling, ant, bokstavordtre->lesantall());



// Del 1 slutt
// --------------------------
// Del 2 - fylle opp statistikkene

	// Core dump pga en av .tell-kallene: Kan dette være pga [k+2] ? 

	// Listestatene opprettes med kapasitet lik antall ord, men de kan kanskje sorteres og 'kappes' seinere slik at kun signifikante frekvenser tas med? 
	listestat* forste_ord = new listestat(0, bokstavordtre->ordlager->les_antall());   // Skulle hatt lengde lik ant. setninger?
	listestat* andre_ord = new listestat(0, bokstavordtre->ordlager->les_antall()); 
	listestat* tredje_ord = new listestat(0, bokstavordtre->ordlager->les_antall()); 

	listestat* pre_komma = new listestat(0, bokstavordtre->ordlager->les_antall());  
	listestat* post_komma = new listestat(0, bokstavordtre->ordlager->les_antall());  

	listestat* siste_ord = new listestat(0, bokstavordtre->ordlager->les_antall());  
	listestat* nest_siste_ord = new listestat(0, bokstavordtre->ordlager->les_antall());  
	listestat* nest_nest_siste_ord = new listestat(0, bokstavordtre->ordlager->les_antall());
	
	// JH, la til tall som komma stats
	listestat* pre_tall = new listestat(0, bokstavordtre->ordlager->les_antall());
	listestat* post_tall = new listestat(0, bokstavordtre->ordlager->les_antall());
	
	printf("\nLengde på 'tekst' (maks k-verdi): %i \n", ant_ord);


	int s = 1;   // 'setningsiterator'  - setningenes endepunkter på hver indeks (punkter i 'data')
	int k =0;   // indeks i 'tekst' - ett ord per indeks
	long l =0;   // indeks som holder orden på posisjon i forhold til 'data', inkrementeres med ordlengden (bokstav, nonord og tallord) i hver iterasjon. Når den overgår setningens endepunkt er vi i ny setning. 

	ord *o = tekst[k];   // Gjeldende ord
	ord *forrige = tekst[k];    // Forrige ord
	bool ny_setning = true;   // settes true når l blir større enn enden på setningen vi er i. 
	bool forste_passert = false;
	bool andre_passert = false;
	bool komma_passert = false;
	bool tall_passert = false;
	int forrige_i = 0;    // indeks til forrige ord
	int forrige_i2 = 0;
	int forrige_i3 = 0;
	int ord_i_setning = 0;
	int ord_i_forrige_setning = 0;

	// Holder styr på antall ord i setningene slik at de kan brukes i estimat av entropi 
	// Hva gjør dekoder for å vite at vi er på 3. siste ord i setning? Kanskje ikke relevant for oss å tenke på? (entropi estimatet er uavhengig av implementasjon)
	int setningslengder[ant_setn];

	bool debug = false;
	bool bruk_cut_off = false;  // Dersom 'false' vil alle ord telles selv om de ikke forekommer så mange ganger.
	// Om satt til 'true' vil kun ord som forekommer ofte nok til at de til sammen utgjør x% (80%) av 'ordvolumet' bli talt opp. 
	// 'false' her er kanskje en forutsetning for å kunne bruke geometrisk gejjnomsnitt ved koding av sannsynlighetene ?

	// Merk: F.eks "Mrs." i start av setning vil la setningen bestå kun av dette ordet i delingen fra icu.
		// Jobbe utifra prioritet? Eller legge ord i flere statistikker - kanskje like greit?
		// 1: Første, 2: siste, 3: andre, 4: tredje  5: nest siste  6: nest-nest siste
		// eller la ord legges til f.eks både i første og i nest-nest-siste. 
		// Fines det måte å måle/vise at det er fornuftig løsning? Tar kanskje tid, men nøkkeltall som
		// printes ut for hver listestat kan kanskje brukes for å gi en indikasjon? 

	// Potensielt mønster å tillatte overlapp for ord på samme avstand fra første/siste. 
		// Eg: 1 ord: Første/siste. 
		// 2 ord: Første - siste
		// 3 ord: Første - andre/nest-sist - sist
		// 4 ord:  Første - andre - nest-sist sist
		// 5 ord: Første - andre - tredje/nest-nest-sist - nest-sist - sist
		// 6 ord: Første - andre - tredje - nestnest-sist - nest-sist - sist

	// MEN for øyeblikket ikke satt stopper for andre og tredje ord telling basert på det over. Stopper kun ved setning kortere enn 3. 
		// Dvs tilnærming uten å endre det (kan jo være hensiktsmessig å ta vare på 1-2-3 rekke): 
		// ((Det som er implementert for øyeblikket:))
		// 1 ord: Første/siste. 
		// 2 ord: Første - andre/siste
		// 3 ord: Første - andre/nest-sist - tredje/sist
		// 4 ord: Første - andre/nest-nest-sist - tredje/nest-sist sist    Forskjell fra over å tillatte både andre og nest-nest sist
		// 5 ord: Første - andre - tredje/nest-nest-sist - nest-sist - sist
		// 6 ord: Første - andre - tredje - nestnest-sist - nest-sist - sist
		// Her innfører vi en slags asymmetri som antar at språket er litt mer forutsigbart fremlengs enn baklengs - er det ok? 
		// - eller bør vi la nest-siste og nest-nest siste få overlappe med første? 
						


	// Burde vi refaktorere og utnytte icu-biblioteket i større grad? 
	while ( s <= ant_setn && k < ant_ord){  // Går til enden av teksten 
	// - alle grenser bør dobbeltsjekkes! 
	
		if(debug) {
			printf("\n\nSetning: %i :  ", s+1);
			skriv(setninger[s-1], setninger[s]-1);
			printf("\n");
		}
		

		while ( l < setninger[s]) { // går fra 0 til enden av første setning, deretter fra neste verdi til enden på neste setning osv.

			if(o->typ == 1) {  // Bokstavord

				++ord_i_setning;


				if (ny_setning) {
					if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
						forste_ord->tell(bokstavordtre->oppslag(o->fra, o->til));
						if (debug && bruk_cut_off) printf("T(1.)     - %7i - ",  o->antall );
					} else {
						if (debug && bruk_cut_off) printf("- %7i - ",  o->antall );
					}

					forste_passert = true;
					andre_passert = false;
					komma_passert = false;
					tall_passert = false;

					if (debug) {
						printf("Første ord:  ");
						skriv(o->fra, o->til);
						printf("\n");
					}
					

					if (s > 1) {  // Bytte ut med en do -- while ? 

						if (!bruk_cut_off || forrige->antall >= cut_off_verdt_telling ) {
							siste_ord->tell(bokstavordtre->oppslag(forrige->fra, forrige->til)); 	
							if (debug && bruk_cut_off) printf("T(s)      - %7i - ",  forrige->antall );
						} else {
							if (debug && bruk_cut_off) printf("- %7i - ",  forrige->antall );
						}

						if (debug) {
							printf("Siste ord:   ");
							skriv(forrige->fra, forrige->til); 
							printf("\n");
						}

						// Strengt tatt ikke nødvendig å sjekke om ordet på denne indeksen er et bokstavord
						// - indeksen ble oppdatert i bokstavord-loop. 
					
						if (debug && ord_i_forrige_setning <= 6) {
							printf("----- Forrige setning bestod av 6 ord eller mindre!! -----\n");
						}
						
						if (tekst[forrige_i2]->typ ==1 && ord_i_forrige_setning > 2) { 
							// 3 ord: Første - andre/nest-sist - tredje/sist
							if (!bruk_cut_off || tekst[forrige_i2]->antall >= cut_off_verdt_telling ) {
								nest_siste_ord->tell(bokstavordtre->oppslag(tekst[forrige_i2]->fra, tekst[forrige_i2]->til));
								if (debug && bruk_cut_off) printf("T(nS)     - %7i - ",  tekst[forrige_i2]->antall );
							} else {
								if (debug && bruk_cut_off) printf("- %7i - ",  tekst[forrige_i2]->antall );
							}

							if (debug) {
								printf("Nest siste ord:  ");
								skriv(tekst[forrige_i2]->fra, tekst[forrige_i2]->til);
								printf("\n");								
							}

						} 
					
						if (tekst[forrige_i3]->typ == 1 && ord_i_forrige_setning > 3) { // sjekk om i rett setning. 
							// 4 ord: Første - andre/nest-nest-sist - tredje/nest-sist sist
							if (!bruk_cut_off || tekst[forrige_i3]->antall >= cut_off_verdt_telling ) {
								nest_nest_siste_ord->tell(bokstavordtre->oppslag(tekst[forrige_i3]->fra, tekst[forrige_i3]->til));
								if (debug && bruk_cut_off) printf("T(nnS)    - %7i - ",  tekst[forrige_i3]->antall );
							} else {
								if (debug && bruk_cut_off) printf("- %7i - ",  tekst[forrige_i3]->antall );
							}

							if (debug) {
								printf("Nest-nest siste ord:  ");
								skriv(tekst[forrige_i3]->fra, tekst[forrige_i3]->til);
								printf("\n\n");	
							}
						}
					}
				
					ny_setning = false;

				} else {

					if (forrige->hyppig >= 0) {
						if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
							bokstavordtre->folger_stat[forrige->hyppig]->tell(bokstavordtre->oppslag(o->fra, o->til));
						}						
					}
					
					if (forste_passert) {
						if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
							andre_ord->tell(bokstavordtre->oppslag(o->fra, o->til));
							if (debug && bruk_cut_off) printf("T(2.)     - %7i - ",  o->antall );
						} else {
							if (debug && bruk_cut_off) printf("- %7i - ",  o->antall );
						}

						if (debug) {
							printf("Andre ord:  ");
							skriv(o->fra, o->til);
							printf("\n");
						}	

						forste_passert = false;
						andre_passert = true;

					} else if (andre_passert) {
						if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
							tredje_ord->tell(bokstavordtre->oppslag(o->fra, o->til));  // Hva om setning bare har ett eller to ord?
							if (debug && bruk_cut_off) printf("T(3.)     - %7i - ",  o->antall );
						} else {
							if (debug && bruk_cut_off) printf("- %7i - ",  o->antall );
						}

						if (debug) {
							printf("Tredje ord:  ");
							skriv(o->fra, o->til);
							printf("\n");
						}	
						andre_passert = false;
					}


					if (komma_passert) {

						if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
							post_komma->tell(bokstavordtre->oppslag(o->fra, o->til)); 
							if (debug && bruk_cut_off) printf("T(postK)  - %7i - ",  o->antall );
						} else {
							if (debug && bruk_cut_off) printf("- %7i - ",  o->antall );
						}

						if (debug) {
							printf("Ord etter komma: ");
							skriv(o->fra, o->til);
							printf("\n");
						}
			
						komma_passert = false;
					}

					if (tall_passert) {
						if (!bruk_cut_off || o->antall >= cut_off_verdt_telling ) {
							post_tall->tell(bokstavordtre->oppslag(o->fra, o->til));
							if (debug && bruk_cut_off) printf("T(postT)  - %7i - ",  o->antall );
						} else {
							if (debug && bruk_cut_off) printf("- %7i - ",  o->antall );
						}

						if (debug) {
							printf("Ord etter tall: ");
							skriv(o->fra, o->til);
							printf("\n");
						}

						tall_passert = false;
					}

				}


				forrige = tekst[k];   // Bytt ut med o ? (Se også om denne bør flyttes)
				forrige_i3 = forrige_i2;
				forrige_i2 = forrige_i;

				forrige_i=k;

			// Undersøk mulige løsninger for ord midt mellom to komma - telles dobbelt nå, men det er kanskje ikke et problem? 
			} else if (o->typ == 0 && data[o->fra] == ',') {   // Ikke på bokstavord, på komma. 

				komma_passert = true;  

				if(forrige->typ == 1) {
					if (!bruk_cut_off || forrige->antall >= cut_off_verdt_telling ) {
						pre_komma->tell(bokstavordtre->oppslag(forrige->fra, forrige->til)); 
						if (debug && bruk_cut_off) printf("T(preK)   - %7i - ",  forrige->antall );
					} else {
						if (debug && bruk_cut_off) printf("- %7i - ",  forrige->antall );
					}

					if (debug) {
						printf("Ord før komma: ");
						skriv(forrige->fra, forrige->til);
						printf("\n");
					}
									
				}
			} else if (o-> typ == 2) {

				tall_passert = true;
					if (!bruk_cut_off || forrige->antall >= cut_off_verdt_telling ) {
					pre_tall->tell(bokstavordtre->oppslag(forrige->fra, forrige->til));
						if (debug && bruk_cut_off) printf("T(preT)   - %7i - ",  forrige->antall );
					} else {
						if (debug && bruk_cut_off) printf("- %7i - ",  forrige->antall );
					}

				if (forrige->typ == 1) {
					if(debug)  {
						printf("Tallord: ");
						skriv(o->fra, o-> til);
						printf("\n");
						printf("Ord før tall: ");
						skriv(forrige->fra, forrige->til);
						printf("\n");
					}

				}
			}
			
			l+=o->lengde();   // Overflow-fare???
			o = tekst[++k];
		}
	
		setningslengder[s-1] = ord_i_setning;
		if (debug) printf("Setning nr. %4i hadde:  %3i ord\n", s-1, ord_i_setning);   // OBS: telle både bokstav og tallord
		++s;
		ny_setning = true;
	
	
		if (ord_i_setning != 0)  ord_i_forrige_setning = ord_i_setning;
		ord_i_setning = 0;
		
	}



// Del 2 slutt
// --------------------------
// Del 3 - Skrive ut kort info om statistikk og skrive til fil om  'bool skriv_til_fil = true;'


	printf("\n\nFerdig med å fylle listestat, nå skrives det til fil dersom 'bool skriv_til_fil == true'. \n");
	bool skriv_til_fil = false;

	forste_ord->akkumuler();
	andre_ord->akkumuler();
	tredje_ord->akkumuler();
	pre_komma->akkumuler();
	post_komma->akkumuler();
	nest_nest_siste_ord->akkumuler();
	nest_siste_ord->akkumuler();
	siste_ord->akkumuler();
	pre_tall->akkumuler();
	post_tall->akkumuler();


	forste_ord->finnmax();	
	andre_ord->finnmax();
	tredje_ord->finnmax();	
	pre_komma->finnmax();	
	post_komma->finnmax();
	nest_nest_siste_ord->finnmax();
	nest_siste_ord->finnmax();
	siste_ord->finnmax();
	pre_tall->finnmax();
	post_tall->finnmax();
	

	// Noen eksempelverdier
	printf("\nFørste ord:           Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", forste_ord->fsum, forste_ord->max_f, forste_ord->max_f2);
	printf("\nAndre ord:            Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", andre_ord->fsum, andre_ord->max_f, andre_ord->max_f2);
	printf("\nTredje ord:           Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", tredje_ord->fsum, tredje_ord->max_f, tredje_ord->max_f2);
	printf("\nFør-komma:            Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", pre_komma->fsum, pre_komma->max_f, pre_komma->max_f2);
	printf("\nEtter-komma:          Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", post_komma->fsum, post_komma->max_f, post_komma->max_f2);
	printf("\nNest-nest siste ord:  Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", nest_nest_siste_ord->fsum, nest_nest_siste_ord->max_f, nest_nest_siste_ord->max_f2);
	printf("\nNest siste ord:       Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", nest_siste_ord->fsum, nest_siste_ord->max_f, nest_nest_siste_ord->max_f2);
	printf("\nSiste ord:            Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", siste_ord->fsum, siste_ord->max_f, siste_ord->max_f2);
	printf("\nFør-tall:             Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", pre_tall->fsum, pre_tall->max_f, pre_tall->max_f2);
	printf("\nEtter-tall:           Totalt:  %7i  Høyeste reksvens:  %6i  Nest høyeste: %6i\n", post_tall->fsum, post_tall->max_f, post_tall->max_f2);



	if (skriv_til_fil) {

		forste_ord->ixsort();	
		andre_ord->ixsort();	
		tredje_ord->ixsort();	
		pre_komma->ixsort();
		post_komma->ixsort();
		nest_nest_siste_ord->ixsort();
		nest_siste_ord->ixsort();
		siste_ord->ixsort();
		pre_tall->ixsort();
		post_tall->ixsort();


		for(int s = 0; s <ant_hyppige_listestat; ++s) {
			bokstavordtre->folger_stat[s]->akkumuler();
			bokstavordtre->folger_stat[s]->ixsort();
			bokstavordtre->folger_stat[s]->finnmax();
		}


	// ---   Skrive til filer:    ---
	// p er sortert statistikk over ord som følger de hyppigste ordene.  raw er ikke sortert slik at frekvensene på hver rad tilhører ordet med samme ordnummer! 
		FILE* p = fopen("statistikk.txt","w"); 
		FILE* p_raw = fopen("raw_stat.txt", "w");

		// pos og pos_raw er som p og raw men for statistikker relatert til posisjonen til ord i setningene. (ikke følger-ord). 
		FILE* pos = fopen("pos_statistikk.txt","w"); 
		FILE* pos_raw = fopen("pos_raw_stat.txt", "w");

		// Legge til antall hyppige og cut-off frekvens øverst i fil
		fprintf(p, "%i/%i/%i\n", ant_hyppige_listestat, sum_hyppigste, bokstavordtre->lestotal());   // integer div?
		fprintf(p_raw, "%i/%i/%i\n", ant_hyppige_listestat, sum_hyppigste, bokstavordtre->lestotal());
		fprintf(pos, "%i/%i/%i\n", ant_hyppige_listestat, sum_hyppigste, bokstavordtre->lestotal());
		fprintf(pos_raw, "%i/%i/%i\n", ant_hyppige_listestat, sum_hyppigste, bokstavordtre->lestotal());

		// OBS: Vet ikke om det er perfekt match over frekvensene enda. Vet i hvert fall at det er mismatch for 
		// før/etter komma bla. på grunn av 'inneklemte' ord mellom to komma, men finnes sikkert flere spesialtilfeller, også i de andre statistikkene?

		fprintf(p, "global");  // global er frekvensene for hvert ord i hele filen
		fprintf(p_raw, "global");

		int fra = 0;
		int til =0;

		// Loop gjennom ordene som har følgere for p og p_raw  (headere) -> statistikk.txt og raw_stat.txt
		for (int head = 0; head < ant_hyppige_listestat; ++head) {
			fra = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[head])->fra;
			til = bokstavordtre->ordlager->hent(bokstavordtre->ordfordeling->ix[head])->til;
			fputc(',', p);
			fputc(',', p_raw);
			if (fra>til) printf("FEIL");
			else {
				while (fra <= til) {
					fputc(data[fra],p_raw);
					fputc(data[fra++],p);
				} 
			}
		}
		fputc('\n', p);
		fputc('\n', p_raw);

		// Blir headere i statistikk for ord på visse posisjoner i setning. 
		fprintf(pos, "global,forste,andre,tredje, nest-nest-siste,nest-siste,siste,pre-komma,post-komma,pre-tallord,post-tallord\n");
		fprintf(pos_raw, "global,forste,andre,tredje, nest-nest-siste,nest-siste,siste,pre-komma,post-komma,pre-tallord,post-tallord\n");
		
		// Loop over index i listestat for å fylle filene med frekvenser
		for (int y = 0; y < bokstavordtre->lesantall(); ++y) {
			fprintf(p, "%i", bokstavordtre->ordfordeling->tab[bokstavordtre->ordfordeling->ix[y]]);
			fprintf(p_raw, "%i", bokstavordtre->ordfordeling->tab[y]);
			fprintf(pos, "%i", bokstavordtre->ordfordeling->tab[bokstavordtre->ordfordeling->ix[y]]);
			fprintf(pos_raw, "%i", bokstavordtre->ordfordeling->tab[y]);
			
			for(s=0; s < ant_hyppige_listestat; ++s) {
				fprintf(p, ", %i", bokstavordtre->folger_stat[s]->tab[bokstavordtre->folger_stat[s]->ix[y]]);
				fprintf(p_raw, ", %i", bokstavordtre->folger_stat[s]->tab[y]);
			}
			fputc('\n', p);
			fputc('\n', p_raw);
			
			fprintf(pos, ", %i, %i, %i, %i, %i, %i, %i, %i, %i, %i\n", 
				forste_ord->tab[forste_ord->ix[y]], 
				andre_ord->tab[andre_ord->ix[y]],
				tredje_ord->tab[tredje_ord->ix[y]], 
				nest_nest_siste_ord->tab[nest_nest_siste_ord->ix[y]],
				nest_siste_ord->tab[nest_siste_ord->ix[y]], 
				siste_ord->tab[siste_ord->ix[y]],
				pre_komma->tab[pre_komma->ix[y]],
				post_komma->tab[post_komma->ix[y]],
				pre_tall->tab[pre_komma->ix[y]],
				post_tall->tab[post_komma->ix[y]]
			);
			
			fprintf(pos_raw, ", %i, %i, %i, %i, %i, %i, %i, %i, %i, %i\n", 
				forste_ord->tab[y],
				andre_ord->tab[y],
				tredje_ord->tab[y],
				nest_nest_siste_ord->tab[y], 
				nest_siste_ord->tab[y],
				siste_ord->tab[y],
				pre_komma->tab[y],
				post_komma->tab[y],
				pre_tall->tab[y],
				post_tall->tab[y]
			);
		}

		fclose(p); 
		fclose(p_raw); 
		fclose(pos); 
		fclose(pos_raw); 

	}
	


// Del 3 slutt
// --------------------------
// Del 4 -Kalle metode for beregning av entropi basert på statistikkene.
	
	
	bool entropi = true;

  	printf("Antall hyppigste ord med folger-stat:  %i\n", ant_hyppige_listestat); //std::end(bokstavordtre->folger_stat) - std::begin(bokstavordtre->folger_stat));

	if (entropi) beregn_entropi(forste_ord, andre_ord, tredje_ord, nest_nest_siste_ord, nest_siste_ord, siste_ord, pre_komma, post_komma, bokstavordtre->folger_stat, ant_hyppige_listestat, setningslengder);


}


// 

// Ende på hs_stat2 
// ===================================================================  HS

// HS: Egen metode for bruk i bacheloroppgaven, ikke som varig del av programmet. For å estimere effekt av statistikker. 
void tekstkompressor::beregn_entropi(listestat *forste_ord, listestat *andre_ord, listestat *tredje_ord, 
listestat *nest_nest_siste, listestat *nest_siste, listestat *siste, listestat *pre_komma, listestat *post_komma, listestat **folger_stat, int ant_folger_listestat, int *setningslengder) {
	printf("\nBeregning av entropi ved gjennomgang av 'tekst'... \n\n");
	// 1., 2., 3., før/etter komma, før/etter tallord, folgerstats

	
	// OBS: Dersom vi forsøker geometisk gjennomsnitt ved treff i flere kontekster, må alle ord telles opp, 
	// dvs. ingen cut-off for hvilke ord det er verdt å telle opp, ellers vil de kodes med 0 sannsynlighet og utelukkes. 
	// OBS: tenk over datatyper her, og på hvilket 'tidspunkt' vi adderer/ganger ol. Både med tanke på presisjon (og riktighet) og effektivitet.


	// Husk å få med symboler... 
	double entropi_bidrag = 0.0;
	double entropi_bidrag_uten_stat = 0.0;
	int midlertidig_teller = 0;

	int setn_nr = 0;
	int l = 0;  // Finne bedre løsning enn dette?...
	int pos_fra_start = 0;
	int pos_fra_slutt = 0;
	bool for_komma;
	bool etter_komma;

	int ord_nr_i_setning = 0;
	ord *o = tekst[0];
	ord *forrige = o;

	bool db = false;       // <=== DEBUG MODE
	bool db2 = false;
	bool bruk_kontekster = true;
	int i =0;

	double sanns_kontekst[ant_folger_listestat + 8];

	while (i < ant_ord) {    // <= ant_ord
		//printf("Setninger[%i] = %i  Setninger[%i] = %i  (l=%i)\n", setn_nr, setninger[setn_nr], setn_nr+1, setninger[setn_nr +1], l);

		if(db) {

			if (o->typ == 0) printf("\nsetning: %i (%i)  l: %i Ord-type: %i  frekvens: %i / %i (nonord)     ", setn_nr, setningslengder[setn_nr], l, o->typ, o->antall, nonordtre->lestotal());
			if (o->typ == 1) printf("\nsetning: %i (%i)  l: %i Ord-type: %i  frekvens: %i / %i (bokstavord) ", setn_nr, setningslengder[setn_nr], l, o->typ, o->antall, bokstavordtre->lestotal());
			if (o->typ == 2) printf("\nsetning: %i (%i)  l: %i Ord-type: %i  frekvens: %i / %i (tallord)    ", setn_nr, setningslengder[setn_nr], l, o->typ, o->antall, tallordtre->lestotal());
		
			skriv(o->fra, o->til);
			printf("\n");
		}

		l += tekst[i]->lengde();

		if (l >= setninger[setn_nr+1]) {
			if (db) printf("\n\nSetninger[%i] = %i  Setninger[%i] = %i  (l=%i)\n", setn_nr, setninger[setn_nr], setn_nr+1, setninger[setn_nr +1], l);
			setn_nr++;
			ord_nr_i_setning =0;
		
		} 
	

		bool i_kontekst = false;

		double sanns = 0.0;


		//printf("setningsl: %i\n", setningslengder[i]);
		//printf("\n");

		if (o->typ == 1) {  // bokstavord   // Inkluder tallord etterhvert!
			++ord_nr_i_setning;

			// ordnr i setning -> første/siste avstand
			 // Obs håndter om i listestat eller IKKE i listestat (er den da 0?)
			if (ord_nr_i_setning == 1) {  // Første ord
				if (db) {
					printf("+++ Førsteord: akk = %i   ", forste_ord->akk[bokstavordtre->oppslag(o->fra, o->til)]);
					skriv(o->fra, o->til);
					printf("\n");
				}
				sanns_kontekst[0] = ((double) forste_ord->tab[bokstavordtre->oppslag(o->fra, o->til)] / forste_ord->fsum);
				if(db2) printf("sanns(1.) kontekst:  %i  / %i  sanns:  %f\n", forste_ord->tab[bokstavordtre->oppslag(o->fra, o->til)], forste_ord->fsum, sanns_kontekst[0]);    // sanns_kontekst[0]);

				entropi_bidrag -= sanns_kontekst[0]; //  bokstavordtre->lesantall() / tekst[i]->antall;  //- 0.1 * log2(0.1);
				++midlertidig_teller;
				
				i_kontekst = true;
			}  // else if nr 2 el 3 -> 2. og 3. ord listestat


			if (ord_nr_i_setning == setningslengder[setn_nr]) {  // siste ord
				// 
				if (db)  {
					printf("+++ Siste ord! :  ");
					skriv(o->fra, o->til);
					printf("\n");
				}
				sanns_kontekst[5] = ((double) siste->tab[bokstavordtre->oppslag(o->fra, o->til)] / siste->fsum);
				if(db2) printf("sanns(S) kontekst:  %i  / %i  sanns:  %f\n", siste->tab[bokstavordtre->oppslag(o->fra, o->til)], siste->fsum, sanns_kontekst[5]);

				entropi_bidrag -= sanns_kontekst[5]; 
				++midlertidig_teller;

				i_kontekst = true;
			} else if (setningslengder[setn_nr] - ord_nr_i_setning == 1) { // Nest siste ord
				if (db) {
					printf("+++ Nest-Siste ord! :  ");
					skriv(o->fra, o->til);			
					printf("\n");
				}

				sanns_kontekst[4] = ((double) nest_siste->tab[bokstavordtre->oppslag(o->fra, o->til)] / nest_siste->fsum);
				if(db2) printf("sanns(nS) kontekst:  %i  / %i  sanns:  %f\n", nest_siste->tab[bokstavordtre->oppslag(o->fra, o->til)], nest_siste->fsum, sanns_kontekst[4]);   

				entropi_bidrag -= sanns_kontekst[4]; 
				++midlertidig_teller;
				i_kontekst = true;

			}  else if (setningslengder[setn_nr] - ord_nr_i_setning == 2) {  // Nest-nest-siste ord
				if (db) {
					printf("+++ Nest-Nest-Siste ord! :  ");
					skriv(o->fra, o->til);
					printf("\n");
				}

				sanns_kontekst[3] = ((double) nest_nest_siste->tab[bokstavordtre->oppslag(o->fra, o->til)] / nest_nest_siste->fsum);
				if(db2) printf("sanns(nnS) kontekst:  %i  / %i  sanns:  %f\n", nest_nest_siste->tab[bokstavordtre->oppslag(o->fra, o->til)], nest_nest_siste->fsum, sanns_kontekst[3]);   

				entropi_bidrag -= sanns_kontekst[3]; 
				++midlertidig_teller;
				i_kontekst = true;
			} 


			if (!i_kontekst) {
				sanns = tekst[i]->antall / bokstavordtre->lesantall();
				// Tenkte egentlig å gange, for log(a) + log(b) = log(a*b) sant?
				entropi_bidrag -= log2(((double) o->antall / bokstavordtre->lesantall()));
				//entropi_bidrag += (log2(bokstavordtre->lesantall()) - log2(tekst[i]->antall)); //  bokstavordtre->lesantall() / tekst[i]->antall;  //- 0.1 * log2(0.1);
				++midlertidig_teller;
			}


			// Beregne geometrisk gjennomsnitt vha. sanns_kontekst[]. 

			entropi_bidrag_uten_stat -= log2(((double) o->antall / bokstavordtre->lesantall()));


			i_kontekst = false;

		}  // bokstavord


		if (o->typ == 0) {  // nonord   
			//entropi_bidrag -= log2(((double) o->antall / nonordtre->lesantall()));

			//entropi_bidrag_uten_stat -= log2(((double) o->antall / nonordtre->lesantall()));
		}


		if (o->typ == 2) {  // tallord   
			// entropi_bidrag -= log2(((double) o->antall / tallordtre->lesantall()));
			//entropi_bidrag_uten_stat -= log2(((double) o->antall / tallordtre->lesantall()));
		}	

		if (o->typ == 3) {  // special?
		printf("\n### Spesialord:  ");

		skriv(o->fra, o->til);
		printf("\n");   
		}



		forrige = o;
		o = tekst[++i];
	}

	// Sjekk om siste ord kommer med!

	printf("\n\nEndelig entropibidrag fra ord utenfor kontekst:   %.2f    per ord:  %f\n", entropi_bidrag, entropi_bidrag/midlertidig_teller);
	printf("\nEndelig entropibidrag uten noen statistikk:   %.2f    per (alle, ikke talt enda) ord:  %f\n\n", entropi_bidrag_uten_stat, entropi_bidrag_uten_stat/ant_ord);  //  entropi_bidrag_uten_stat/ant_ord);


}





// =========================================================================== HS


/*
	Metoder for listestat
	*/

void listestat::dump() {
	printf("min_i=%i  max_i=%i max_f=%i  fsum=%i\n",min_i, max_i, max_f, fsum);
	for (int i=min_i;i<=max_i;++i) printf("%5i %9i %9i\n",i, tab[i], akk[i]);
}

//Tell opp
void listestat::tell(unsigned int tall) {
if (dbg) printf("min_i:%i   tall:%i   max_i:%i\t%c\n",min_i,tall,max_i,tall);
	assert(min_i <= tall && tall <= max_i);
	++tab[tall];
}

//Legg til mer enn 1
void listestat::add(unsigned int ix, unsigned int antall) {
if (dbg) printf("min_i: %5i  ix:%i  max_i:%5i\n", min_i, ix, max_i);
	assert(min_i <= ix && ix <= max_i);
	tab[ix] += antall;
}

//Akkumuler. Fyll ut akk[], etter at opptelling er ferdig
//Gjør klar for koding
void listestat::akkumuler() {
	for (int i=1; i<= max_i; ++i) akk[i] = akk[i-1]+tab[i-1];
	fsum = akk[max_i] + tab[max_i];
	akkumulert = true;
}

void listestat::kod(unsigned int x, tekstkompressor *tk) {
	assert(akkumulert);
	tk->kod_intervall(akk[x], akk[x]+tab[x], fsum);
	//Kan evt. oppdatere statistikken her, 
	//for noen få ekstra bits.
}

//Som kod(), men vi (og mottaker) vet at x <= grense, og kan
//dermed utelukke mange alternativer. (Gir færre bits)
void listestat::kod_grense(unsigned int x, unsigned int grense, tekstkompressor *tk) {
	if (grense >= max_i) kod(x, tk); //grensen hjalp ikke
	else tk->kod_intervall(akk[x], akk[x]+tab[x], akk[grense+1]);
}

//Som kod(), men vi vet at 0 ikke kan skje.
//Så sannsynligheten for 0 kan trekkes fra totalen -> mer kompakt.
void listestat::kod_nix0(unsigned int x, tekstkompressor *tk) {
	assert(x > 0); //Fordi 0 jo er ulovlig - hele poenget...
	tk->kod_intervall(akk[x]-tab[0], akk[x]+tab[x]-tab[0], fsum-tab[0]);
}

//Finne max_f og max_f2, etter at statistikk er talt opp
//typisk før til_fil 
void listestat::finnmax() {
	max_f = max_f2 = 0;
	for (int i = min_i; i <= max_i; ++i) {
		if (tab[i] > max_f) {
			max_f2 = max_f;
			max_f = tab[i];
		} else if (tab[i] > max_f2) max_f2 = tab[i];
	}
}

/*
	 Metoder for klassen «trenode»
	 */
trenode::trenode (int onr) {
	v = h = NULL;
	ordnr = onr;
}

void trenode::skriv_inorden(statistikk **stats, ordlager_c *ordlager) {
	static ord *forrige = NULL;
	static int f_like = 0;
	if (v) v->skriv_inorden(stats, ordlager);
	ord *o = ordlager->hent(ordnr);
	skriv(o->fra, o->til);
	int len = o->til-o->fra+3;
	if (len < 27) printf("%s", &"                            "[len]);
	printf("\t#:%9i",o->antall);
	if (forrige && stats) {
		//Finn ant. like
		int ff = forrige->fra;
		int of = o->fra;
		int ft = forrige->til;
		while (ff <= ft && data[ff] == data[of]) { ++ff; ++of; }
		//of er nå på første ulike.
		int like = of-o->fra;
		//antall like, og rel. til forr.
		printf("   a:%2i  r:%3i", like, like-f_like);
		stats[0]->reg(like);
		stats[1]->reg(like-f_like);
		f_like = like;
		//bokstavøking, hvis ikke overlapp med hele forrige ord:
		if (ff <= ft && data[of]-data[ff] > 0) {
			printf("   bokst:%2i ", data[of]-data[ff]);
			stats[2]->reg(data[of]-data[ff]);
		}
	}
	putchar('\n');
	forrige = o;
	if (h) h->skriv_inorden(stats, ordlager);
}

//Gjør et tre om til liste, for enklere prosessering videre
//parametre: liste og telleverk
void trenode::tre_til_liste(int *liste, int *nr) {
	if (v) v->tre_til_liste(liste, nr);
	liste[(*nr)++] = ordnr;
	if (h) h->tre_til_liste(liste, nr);
}


ordstakk::ordstakk(int kap) {
	kapasitet = kap;
	tab = new ord *[kap];
	sp = 0;
}

ordstakk::~ordstakk() {
	delete tab;
}

inline void ordstakk::push(ord *o) {
	if (sp == kapasitet) throw("Overfull stakk.");
	tab[sp++] = o;
}

inline ord *ordstakk::pop() {
	return (sp == 0) ? NULL : tab[--sp];
}


//Sammenligning for å sortere ord i stigende orden, men korte først:
//a,b,c,aa,ab,ac,ba,bc,ca,cb,aaa,bbb,ccc,abcd,adda, ...
//Formidle lengden på ord, uten å måtte overføre en liste.
//Først alle med lengde 1, så alle med lengde 2...
int ordtre::sammenlign_shortlex(int fra, int const til, ord *o) {
	int ofra = o->fra;
	int const otil = o->til;
	int diff = (til-fra) - (otil-ofra); //Først, evt. lengdeforskjell
	while (!diff) {
		if (fra > til) return diff; //Like ord
		diff = data[fra]-data[ofra];
		++fra;
		++ofra;
	}
	return diff;
}


//Sammenligning for å sortere ord i stigende orden
int ordtre::sammenlign1(int fra, int const til, ord *o) {
	int ofra = o->fra;
	int const otil = o->til;
	int diff;
	while (fra <= til && ofra <= otil) {
		diff = data[fra]-data[ofra];
		if (diff) return diff; //Bryt på ulikt tegn
		++fra;
		++ofra;
	}
	//Ordene matchet tegn for tegn, men er de like lange?
	return (til-fra) - (otil-ofra); //lengde på nyord minus lengde på lagret ord
}

//Baklengs sammenligning, for å finne suffix
int ordtre::sammenlign_bakl(int const fra, int til, ord *o) {
	int const ofra = o->fra;
	int otil = o->til;
	int diff;
	while (fra <= til && ofra <= otil) {
		diff = data[til]-data[otil];
		if (diff) return diff; //Bryt på ulikt tegn
		--til;
		--otil;
	}
	//Ordene matchet tegn for tegn, men er de like lange?
	return (til-fra) - (otil-ofra); //lengde på nyord minus lengde på lagret ord
}

//Konstruktører
ordtre::ordtre(char const * const nytt_navn, ordlager_c *olager, functype sammenlignf, tekstkompressor *tk) {
	navn = nytt_navn;
	total = maxrep = single = maxlengde = 0;
	ordlager = olager;
	sammenlign = sammenlignf; //spesifisert sammenling.
	tkmp = tk;
	rot = NULL;

	folger_stat = NULL; //HS - i bruk, skal bli liste med en listestat per hyppige ord
	ordfordeling = NULL;   // HS - i bruk - en listestat med frekvensene til alle ord (0.-ordens stat)
}

ordtre::ordtre(char const * const nytt_navn, ordlager_c *olager, tekstkompressor *tk) {
	navn = nytt_navn;
	total = maxrep = single = maxlengde = 0;
	ordlager = olager;
	sammenlign = &ordtre::sammenlign1; //std. sammenlign
	tkmp = tk;
	rot = NULL;

	folger_stat = NULL; //HS
	ordfordeling = NULL;  //HS

}

//Merk alle ordene med kategori.
//Legg til offset på ordnumre.

void ordtre::merk(int merke, int offset) {
	for (int i = 0; i < lesantall(); ++i) {
		ord *o = ordlager->hent(i);
		o->typ = merke;
		o->nr += offset;
	}
}

//Legger ord inn i et tre, med lavest antall først.
//(For like antall: bruk sammenligningsmetoden i treet)
//ordene må finnes i lageret fra før
void ordtre::nyord3(int ordnr) {
	if (!rot) {
		rot = new trenode(ordnr);
		return;
	}
	trenode *n = rot;
	ord *o = ordlager->hent(ordnr); //nytt ord
	do {
		ord *n_ord = ordlager->hent(n->ordnr);
		int res = o->antall - n_ord->antall;
		if (!res) res = (this->*sammenlign)(o->fra, o->til, ordlager->hent(n->ordnr));
		assert(res);
		if (res > 0) {
			if (n->h) n = n->h; else {
				n->h = new trenode(ordnr);
				return;
			}
		} else {
			if (n->v) n = n->v; else {
				n->v = new trenode(ordnr);
				return;
			}
		}
	} while (1);
}

//Legger inn ord, i et tre med gitt sorteringsmetode. 
//(Baklengs, shortlex, vanl)
//Ordene er ikke nye, de fins i lageret fra før.
void ordtre::nyord2(int ordnr) {
	if (!rot) {
		rot = new trenode(ordnr);
		return;
	}
	trenode *n = rot;
	ord *o = ordlager->hent(ordnr);
	do {
//	int res = sammenlign2(o->fra, o->til, ordlager->hent(n->ordnr));
		int res = (this->*sammenlign)(o->fra, o->til, ordlager->hent(n->ordnr));
		if (!res) {
			//Hva gikk galt?
			skriv(o->fra, o->til);
			printf("\tordnr=%i, n->ordnr:%i\n", ordnr,n->ordnr);
			fflush(stdout);
		}
		assert(res);
		if (res > 0) {
			if (n->h) n = n->h; else {
				n->h = new trenode(ordnr);
				return;
			}
		} else {
			if (n->v) n = n->v; else {
				n->v = new trenode(ordnr);
				return;
			}
		}
	} while (1);
}

//Finner et ord som starter på et bestemt tegn.
//Ment for å finne ett-tegns ord som «<»
ord *ordtre::tegnoppslag(char c) {
	if (!lesantall()) return NULL;
	trenode *n = rot;
	do {
		if (!n) return NULL;
		ord *o = ordlager->hent(n->ordnr);
		int res = (int)c - (int)data[o->fra];
		if (!res) return o;
		if (res > 0) n = n->h; else n = n->v;
	} while(1);
}

//Finner et ord, og returnerer nr. Evt. -1 hvis det ikke fins
int ordtre::oppslag(int fra, int til) {
	if (!lesantall()) return -1;
	trenode *n = rot;
	do {
		int res = (this->*sammenlign)(fra, til, ordlager->hent(n->ordnr));
		if (!res) return n->ordnr;
		if (res > 0) {
			//h.side
			if (n->h) n = n->h; else return -1;
		} else {
			//v.side
			if (n->v) n = n->v; else return -1;
		}
	} while (1);
}

//Legger et nyfunnet ord inn, eller teller opp hvis det fins fra før:
int ordtre::nyord(int fra, int til, ordtre *baklengstre, int ant) {
	total += ant;
	if (!lesantall()) {
		rot = new trenode(ordlager->nyord(fra, til, ant));
		tkmp->reg_tekst(ordlager->hent(rot->ordnr));
		if (baklengstre) baklengstre->nyord2(rot->ordnr);
		single = maxrep = 1;
		maxlengde = til - fra + 1;
	} else {
		trenode *n = rot;
		do {
			int res = (this->*sammenlign)(fra, til, ordlager->hent(n->ordnr));
			if (!res) { //fant ordet, tell opp
				ord *o = ordlager->hent(n->ordnr);
				tkmp->reg_tekst(o);
				o->entil(ant);
				if (o->antall == 2) --single;
				if (o->antall > maxrep) maxrep = o->antall;
				return n->ordnr;
			}
			int ordnr;
			if (res > 0) { //Større ord, til høyre
				if (n->h) n = n->h;
				else {
					ordnr = ordlager->nyord(fra, til, ant);
					tkmp->reg_tekst(ordlager->hent(ordnr));
					n->h = new trenode(ordnr);
					if (baklengstre) baklengstre->nyord2(ordnr);
					++single;
					if (til-fra+1 > maxlengde) maxlengde = til-fra+1;//bare 1 gang/ord
					return ordnr;
				}
			} else {       //mindre ord, til v.
				if (n->v) n = n->v;
				else {
					ordnr = ordlager->nyord(fra, til, ant);
					tkmp->reg_tekst(ordlager->hent(ordnr));
					n->v = new trenode(ordnr);
					if (baklengstre) baklengstre->nyord2(ordnr);
					++single;
					if (til-fra+1 > maxlengde) maxlengde = til-fra+1;//bare 1 gang/ord
					return ordnr;
				}
			}
		} while (1);
	}
	return 0; //Første ord er nr 0.
}

	//Skriv ordliste
void ordtre::skrivordliste() {
	printf("\nORDLISTE: %s\n", navn);
	if (!lesantall()) return;
	printf("lengste ord: %i byte\n", maxlengde);
	statistikk *stats[3];
	stats[0] = new statistikk("Ord-overlapp (abs)", 0, maxlengde);  //0..x tegn å kopiere fra forr. ord
	stats[1] = new statistikk("Ord-overlapp (rel)", -maxlengde, maxlengde); //som over, men rel. til forrige ord
	stats[2] = new statistikk("Bokstavforskjeller", 0,254); //bokstavforskjeller
	rot->skriv_inorden(stats, ordlager);

//	stats[0]->rapport();
//	stats[1]->rapport();
//	stats[2]->rapport();
	delete stats[0];
	delete stats[1];
	delete stats[2];
}

//Annet

void print_setn_status(BreakIterator * setn_bi) {/*
	if (setn_bi->getRuleStatus() == UBRK_SENTENCE_TERM) printf("\nSENTENCE_TERM\n");
	else if (setn_bi->getRuleStatus() == UBRK_SENTENCE_SEP) printf("\nSENTENCE_SEP\n");*/
}


void print_ord_status(BreakIterator* ord_bi, int verbose) {
	if (verbose >= 9) putchar('\t');
	switch (ord_bi->getRuleStatus()) {
		case UBRK_WORD_NONE:
			if (verbose >= 9) printf("nonword");
			break;
		case UBRK_WORD_NUMBER:
			//tall samles opp, f.eks. «32,000», «97,500,000», «L1000»
			//«0.3», «Ams80», «22T16», «04T01», «CJames745»
			if (verbose >= 9) printf("tall");
			break;
		case UBRK_WORD_LETTER:
			if (verbose >= 9) printf("bokstav-ord");
			break;
		case UBRK_WORD_KANA:
			if (verbose >= 9) printf("kana-ord");
			break;
		case UBRK_WORD_IDEO:
			if (verbose >= 9) printf("ideogram");
			break;
		default:
			if (verbose >=9) printf("ukjent ord");
			break;
	}
}

const char bruk[] = "icu-pakk [-1..9] innfil utfil\n7: ordlister\n8: alle setninger\n9: hvert eneste ord\n";

int main(int argc, char *argv[]) {
	int verbose = 0;
	char const *innfilnavn;
	char const *utfilnavn;
	switch (argc) {
		case 3:
			if (*argv[1] == '-' || *argv[2] == '-') feil(bruk);
			innfilnavn = argv[1];
			utfilnavn = argv[2];
			break;
		case 4: //godta "-9 inn ut" eller "inn ut -9" 
			if (*argv[1] == '-') {
				innfilnavn = argv[2];
				utfilnavn = argv[3];
				verbose = atoi(argv[1]+1);
			} else if (*argv[3] == '-') {
				innfilnavn = argv[1];
				utfilnavn = argv[2];
				verbose = atoi(argv[3]+1);
			} else feil(bruk);
			break;
		default:
			feil(bruk);
	}

	try {
		tekstkompressor tk(innfilnavn, utfilnavn, true, verbose);
		tk.pakk();
	} catch (char const *meld) { printf("Feil: %s\n", meld); }
}

//sumf_grense, eg. summen av frekvensene (eller øvre grense for denne)
/*
  skriver:
  nøyaktig max_i, begr av maxgrense_i
  nøyaktig max_f, begr av maxgrense_f (ikke god, feiler om maxgrense_f er for stor)
  antar at utpakker VET hva min_i er
  entries, begrenset av max_f og sum-av-gjenværende.
	          sum-av-gjenværende starter på sumf_grense,bør være lik fsum

 sumf_grense BØR være lik sumf. Grunnen til å ikke automatisere skriving 
 av dette, er at mottaker ofte kjenner tallet fordi det er "antall ord"
 som er felles grense for mange ting.

 Hvis mottaker IKKE kjenner sumf, bare skriv den i forkant med kod_tall.
 */
void listestat::til_fil(tekstkompressor *tk, int maxgrense_i, int sumf_grense) {
	finnmax();
	//Max i for tabellen. Utpakker kjenner min i.
if (dbg) printf("%i (%i..%i)\n",max_i, min_i, maxgrense_i);
	tk->kod_tall(max_i, min_i, maxgrense_i);
	//høyeste entry, begr av antall unike ord i lista
	int intervall = max_i - min_i +1;
	//Minste max-entry: hvis fordelingen er jevn. (!!!FORUTSETTER at maxgrense_F ikke er altfor høy!!!
	//Største max-entry: hvis alle entries er på samme tabpos.
if (dbg) printf("%i (%i..%i)\n",max_f,(sumf_grense+intervall-1)/intervall, sumf_grense);
	tk->kod_tall(max_f, (sumf_grense+intervall-1)/intervall, sumf_grense);
	tk->kod_tall(max_f2, 0, max_f);
	//Kod tabellen. min er 0, bortsett fra siste entry hvor min=gjenværende
	//max er minimum av max_f og gjenværende (sumf_grense)
	//bruker max_f2 når max_f har passert.
	int grense_f = max_f;
	for (int i = min_i; i <= max_i; ++i) {
		int grense = (grense_f < sumf_grense) ? grense_f : sumf_grense;
if (dbg) printf("%i (%i..%i)\n",tab[i], 0, grense);
		tk->kod_tall(tab[i], 0, grense);
		if (tab[i] == max_f) grense_f = max_f2; //stram inn!
		sumf_grense -= tab[i];
	}
}

//Som til_fil, men bruker en sorteringsrekkefølge for høyere kompr.
//Forutsetter at mottaker har denne rekkefølgen
void listestat::til_fil_ix(tekstkompressor *tk, int sumf_grense, listestat *ls_ix) {
	finnmax();
	//Dropper max_i, utpakker kjenner ant_ix
	//sumf_grense er antall fra nivå0. egentlig kjent det også.
	if (dbg==1)printf("sumf_grense = %i, fsum= %i\n",sumf_grense,fsum);
	assert(sumf_grense == fsum);
	int grense_f = max_f;
	for (int i = 0; i < ls_ix->ant_ix; ++i) {
		int grense = (grense_f < sumf_grense) ? grense_f : sumf_grense;
		tk->kod_tall(tab[ls_ix->ix[i]], 0, grense);
		if (tab[ls_ix->ix[i]] == max_f) grense_f = max_f2;
		sumf_grense -= tab[ls_ix->ix[i]];
	}
}


int finn_felles_starter(int ant, int ordnr[], ordlager_c *ordlager) {
	ord *o2 = ordlager->hent(ordnr[0]);
	for (int i = 0; i < ant-1; ++i) {
		ord *o1 = o2;
		o2 = ordlager->hent(ordnr[i+1]);
		o1->overlapp = 0;
		int of1 = o1->fra;
		int of2 = o2->fra;
		//Finn overlappende endelser:
		while (data[of1] == data[of2] && of1 <= o1->til) {
			++of1;
			++of2;
			++o1->overlapp;
		} 
	}
	//Finn lengste BRUKBARE serie ( > MINSTESERIE reps)
	int lengste = -1;
	int serie;
	do {
		++lengste;
		serie = 0;
		for (int j=0; j < ant; ++j) {
			ord *o = ordlager->hent(ordnr[j]);
			if (o->overlapp >= lengste) ++serie; else serie = 0;
			if (serie >= MINSTESERIE) break;
		}
	} while (serie >= MINSTESERIE);
	return lengste-1;
}


void finn_felles_endelser(int ant, int ordnr[], ordlager_c *ordlager) {
	ord *o2 = ordlager->hent(ordnr[0]);
	for (int i = 0; i < ant-1; ++i) {
		ord *o1 = o2;
		o2 = ordlager->hent(ordnr[i+1]);
		o1->overlapp2 = 0;
		int ot1 = o1->til;
		int ot2 = o2->til;
		//Finn overlappende endelser:
		while (data[ot1] == data[ot2] && ot1 >= o1->fra) {
			--ot1;
			--ot2;
			++o1->overlapp2;
		} 
	}
}

int finn_lengste_suffix(int ant, int ordnr[], ordlager_c *ordlager) {
	//Finn lengste BRUKBARE suffix ( > MINSTESERIE reps)
	int lengste = -1;
	int serie;
	do {
		++lengste;
		serie = 0;
		for (int j=0; j < ant; ++j) {
			ord *o = ordlager->hent(ordnr[j]);
			if (o->overlapp2 >= lengste) ++serie; else serie = 0;
			if (serie >= MINSTESERIE) break;
		}
	} while (serie >= MINSTESERIE);
	return lengste-1;
}

//Finn prefixer. Unngå ord med lengre prefix, ikke kræsj med suffix
bool finn_prefix2(int lengde, int *ant_prefix, int ant, int *ordnr, ordtre *tre, ordtre *btre) {
	bool ret = false;
	ordlager_c *ordlager = tre->ordlager;
	int intervallstart = -1; //Intet intervall - ennå
	int ant_brukbare = 0;
	for (int i = 0; i < ant; ++i) {
		ord *o = ordlager->hent(ordnr[i]);
		if (intervallstart == -1) {
			//Har ikke intervall, skal vi starte et?
			if (o->overlapp >= lengde && o->endelse >= lengde && o->start2 == -1) {
				intervallstart = i;
				ant_brukbare = 1;
			}
		} else {
			//Har intervall, fortsette eller avbryte?
			if (o->overlapp < lengde || i==ant) {
				//Bryt
				if (ant_brukbare >= MINSTESERIE) {
					//Stort nok intervall f.o.m intervallstart t.o.m i, BRUK det:
					//Reg. endelsen i ordlager o->fra, o->fra+lengde-1
					int prefordnr = tre->nyord(o->fra, o->fra+lengde-1, btre, 0);
					//Lager nytt ord, ELLER nytter et eksisterende som passer!
					//et ord kan bli sitt eget prefix/suffix. Håndteres lenger ned.

					ord *prefixord = ordlager->hent(prefordnr);
					
					if (prefixord->antall == 1) ret = true; //Ordet var nytt, ikke et gammelt et
					for (int j = intervallstart; j <= i; ++j) {
						//Oppdater brukbare ord
						ord *o2 = ordlager->hent(ordnr[j]);
						if (o2->endelse < lengde || o2->start2 != -1) continue;
						if (ordnr[j] == prefordnr) {
							continue;
						}	
						o2->start2 = prefordnr;
						o2->endelse -= (prefixord->lengde() );
					}
					++*ant_prefix;
				}	
				intervallstart = -1; //Avslutt dette, så nyy intervall kan startes
			} else {
				//Fortsett. Kan denne brukes?
				if (o->endelse >= lengde && o->start2 == -1) ++ant_brukbare;
			}
		}
	}
	return ret;
}

//Finner suffixer. Tar hensyn til etablert overlapp.
//Lager ikke nye ord, suffix ligger på egen liste som refererer ordlista
void finn_suffix3(int lengde, int *ant_suffix, int ant, int *ordnr, ordlager_c *ordlager, int *suf_ord, int *suf_len) {
	int intervallstart = -1; //Intet intervall - ennå
	int ant_brukbare = 0;
	for (int i = 0; i < ant; ++i) {
		ord *o = ordlager->hent(ordnr[i]);
		if (intervallstart == -1) {
			//Har ikke intervall, skal vi starte et?
			if (o->overlapp2 >= lengde && o->endelse >= lengde && o->endelse2 == -1) {
				intervallstart = i;
				ant_brukbare = 1;
			}
		} else {
			//Har intervall, fortsette eller avbryte?
			if (o->overlapp2 < lengde || i==ant) {
				//Bryt
				if (ant_brukbare >= MINSTESERIE) { 
					//Stort nok intervall f.o.m intervallstart t.o.m i, BRUK det:
					//I serien kan det være ord som matchet et lengre
					//suffix. Sjekk, så vi ikke lager for mange suffixfrie ord.
					//Ord som er suffixbase merkes med fix & 8, så let etter et slikt.
					int suff_ord = -1;
					for (int j = intervallstart; j <= i; ++j) {
						ord *suf = ordlager->hent(ordnr[j]);
						if (suf->fix & 8) {
							suff_ord = j;
							break; 
						}	
					}
					if (suff_ord == -1) {
						//Lag et suffixord, fordi vi ikke fant noe gammelt
						suff_ord = intervallstart;
						ordlager->hent(ordnr[suff_ord])->fix |= 8;
					}
					//Registrer suffixet:
					suf_ord[*ant_suffix] = ordlager->hent(ordnr[suff_ord])->nr;
					suf_len[*ant_suffix] = lengde;
					
					for (int j = intervallstart; j <= i; ++j) {
						//Oppdater brukbare ord
						ord *o2 = ordlager->hent(ordnr[j]);
						if (o2->endelse < lengde || o2->endelse2 != -1) continue;
						if (o2->fix & 8) continue; //Dropp ord som er base for noe suff.

						o2->endelse2 = *ant_suffix;
						o2->endelse -= lengde;
					}
					++*ant_suffix;
				}	
				intervallstart = -1; //Avslutt dette, så nye intervall kan startes
			} else {
				//Fortsett. Kan denne brukes?
				if (o->endelse >= lengde && o->endelse2 == -1 && !(o->fix & 8)) ++ant_brukbare;
			}
		}
	}

}

//Finn suffixer. Ikke kræsj med prefix, eller ord med lengre suffix
//Lag nyord.
bool finn_suffix2(int lengde, int *ant_suffix, int ant, int *ordnr, ordtre *tre, ordtre *btre) {
	bool ret = false;
	ordlager_c *ordlager = tre->ordlager;
	int intervallstart = -1; //Intet intervall - ennå
	int ant_brukbare = 0;
	for (int i = 0; i < ant; ++i) {
		ord *o = ordlager->hent(ordnr[i]);
		if (intervallstart == -1) {
			//Har ikke intervall, skal vi starte et?
			if (o->overlapp2 >= lengde && o->endelse >= lengde && o->endelse2 == -1) {
				intervallstart = i;
				ant_brukbare = 1;
			}
		} else {
			//Har intervall, fortsette eller avbryte?
			if (o->overlapp2 < lengde || i==ant) {
				//Bryt
				//!!!sjekk om suffixordet fins fra før (flaks)
				//i så fall, lag serien selv om den er for kort.
				if ((ant_brukbare >= MINSTESERIE) || 
					  ((tre->oppslag(o->til-lengde+1, o->til) != -1) && 
						 (ant_brukbare >= MINSTESERIE2)) ) {
					//Stort nok intervall f.o.m intervallstart t.o.m i, BRUK det:
					//Reg. endelsen i ordlager
					int suffordnr = tre->nyord(o->til-lengde+1, o->til, btre, 0);
					//Lager nytt ord, ELLER nytter et eksisterende som passer!
					//et ord kan bli sitt eget prefix/suffix. Håndteres lenger ned.

					ord *suffixord = ordlager->hent(suffordnr);
					if (suffixord->antall == 1) ret = true;
					for (int j = intervallstart; j <= i; ++j) {
						//Oppdater brukbare ord
						ord *o2 = ordlager->hent(ordnr[j]);
						if (o2->endelse < lengde || o2->endelse2 != -1) continue;
						if (ordnr[j] == suffordnr) {
							continue; //Ikke la ordet bli sitt eget suffix
						}	
						o2->endelse2 = suffordnr;
						o2->endelse -= (suffixord->lengde() );
					}
					++*ant_suffix;
				}	
				intervallstart = -1; //Avslutt dette, så nye intervall kan startes
			} else {
				//Fortsett. Kan denne brukes?
				if (o->endelse >= lengde && o->endelse2 == -1) ++ant_brukbare;
			}
		}
	}
	return ret;
}

//Finn suffixer som det er minst 30 av.
//Ikke kræsj med prefix, 
//Ikke bruk entries som allerede har brukt lenger suffix
void finn_suffix(int lengde, int *suffix, int *ant_suffix, int ant, int *ordnr, ordlager_c *ordlager) {
	int intervallstart = -1; //Intet intervall - ennå
	int ant_brukbare = 0;
	for (int i = 0; i <= ant; ++i) {
		ord *o = ordlager->hent(ordnr[i]);
		if (intervallstart == -1) {
			//Har ikke intervall, skal vi starte et?
			if (o->overlapp2 >= lengde && o->endelse >= lengde && o->endelse2 == -1) {
				intervallstart = i;
				ant_brukbare = 1;
			}
		} else {
			//Har intervall, fortsette eller avbryte?
			if (o->overlapp2 < lengde || i==ant) {
				//Bryt
				if (ant_brukbare >= MINSTESERIE) {
					//Stort nok intervall f.o.m intervallstart t.o.m i, BRUK det:
					//Reg. endelsen i ordlager
					int lagernr = ordlager->nyord(o->til-lengde+1, o->til, 0);
					suffix[*ant_suffix] = lagernr;
					ord *suffixord = ordlager->hent(lagernr);
					suffixord->antall = ant_brukbare;
					for (int j = intervallstart; j <= i; ++j) {
						//Oppdater brukbare ord
						ord *o2 = ordlager->hent(ordnr[j]);
						if (o2->endelse < lengde || o2->endelse2 != -1) continue;
						o2->endelse2 = *ant_suffix;
						o2->endelse -= (suffixord->lengde() );
					}
					++*ant_suffix;
					//Videre:
					//Søk i suffixtabellen, se om lengre suffix kan 
					//forkortes med DETTE suffixet... 
					//!!!ikke gjort, men kan komprimere litt mer
				}	
				intervallstart = -1; //Avslutt dette, så nyy intervall kan startes
			} else {
				//Fortsett. Kan denne brukes?
				if (o->endelse >= lengde && o->endelse2 == -1) ++ant_brukbare;
			}
		}
	}
}

//Finn overlapp & bokstavforskjell for skriv6()
void finn_overlapp(int onr, int *ordnr, ordtre *tre) {
	ord *o = &nullord;
	for (int i = 0; i < onr; ++i) {
		ord *forrige = o;
		o = tre->ordlager->hent(ordnr[i]);
		o->nr = i;
		int of = o->fra, ff = forrige->fra;
		while (ff <= forrige->til && data[ff] == data[of]) { ++ff; ++of; }
		//ff og of på første ulike, 
		//eller ff gikk forbi enden.

		if (ff > forrige->til) {
			//ff passerte forrige ord, vi kopierer hele ordet
      o->bokstavforskjell = 0; //Eneste tilfelle hvor det blir 0, kodes ikke.
      o->overlapp = forrige->lengde(); //Dette kan utpakker se også
      o->endelse = o->lengde() - o->overlapp;
    } else {
      //Må ha stoppet pga. av ulikhet, for stopp før slutt på forrige
      o->overlapp = of - o->fra;
      o->bokstavforskjell = data[of] - data[ff];
      o->endelse = o->lengde() - o->overlapp - 1; //-1 for bokstavforskjellen
    }
	} //slutt på for
}

//Skriv statistikk på nivå1, for tegnkompresjon
/*
	Komprimer nivå1-statistikken v.hj.a. nivå0-statistikken.
	Frekvensene i en listestat skrives med grenseverdi lik
	det laveste av maksfrekvensen og summen av gjenv. frekvenser.
  Jo lavere grenser, jo bedre. 
  Antar at fordelingen på nivå1 ligner fordelingen på nivå 0,
  dette har vist seg å stemme brukbart. Sorterer derfor alle nivå1-
  tabellene så rekkefølgen tilsv. synkende orden for nivå0.
  Dermed har disse tabellene stort sett de største tallene først,
  og de minste sist. Grensen på "sum av gjenværende" blir veldig tett,
  og nivå1-statistikken tar mye mindre plass enn de 256k som vi
	får uten slik kompresjon. 4-10x kompresjon (for stats) er vanlig.
	Utpakker har nivå0-stats, og kan derfor unscramble sorteringen
	etter å ha dekodet tabellene.

  I tillegg overføres ikke tabeller som bare inneholder nuller.	
	nivå0-filstørrelse: 1112971
	nivå1-filstørrelse: 1047285
	sparte                65686 > 64kB! Og, ordlista under 1MB totalt!
*/
void tekstkompressor::skrivtxtstat1(listestat *txt, listestat txt1[256]) {
	//Finn hvilke tabeller som skal overføres. (De med fsum > 0)
	//Samme som de som har entryZ0 i nivå0-stat
	//Lag sorteringsrekkefølgen
	txt->ixsort();
	//Skriv nivå1-stats ved hjelp av nivå0-rekkefølgen:
	for (int i=0; i < 256; ++i) if (txt->tab[i]) txt1[i].til_fil_ix(this, txt->tab[i], txt);
}



/*
overlapp, bforskjell, ende, suffix
overlapp begr. av max-overlapp, og lengden på forrige ord.
Men lengden på forrige ord er ukjent, hvis suffix!

Eller, et separat suffixlager. Kan da vite lengdene på suffix.
Bedre: overføre suffixlengdene (i tillegg til posisjoner)
  Dermed vet vi også ordlengde, for de ordene som ER suffix.

Vanlig ord:
overlapp, begr. av lengde på forrige ord (alltid kjent)
bforskjell
ende
suffix


Ord som er suffix:
overlapp, begr. av lengde på forrige ord OG ordlengden
ende, begrenset av hvor mye lengde som er igjen
suffix, plukket fra en begrensa liste

Ny måte å lagre suffix - IKKE som ord.
Egen suffixtabell, hvor hvert suffix angis som "ordnr" og "antall bokstaver på slutten". (Ordet merkes, slik at det IKKE får hektet på suffix selv.)
Forenklinger:
Ingen nye ord, så vi slipper å lage lister på nytt!! Bare:
1. Finn overlapp & endelser - én gang. Som skriv1
2. Loop hvor vi leter opp stadig kortere suffix.
   Nytt suffix: Første ord i serien merkes 
	 (gjerne et som bærer lengre suffix også)
	 resten får suffixet, hvis de ikke har et lengre et fra før

	 suffixene lagres som ordnumre (eller hopp, det har jo virket bra)
	 og en suffixlengde. (Dermed vet mottaker også lengdene på suffixene,
	 og kan beregne lengden på hvert ord når det er dekodet. Og det
	 blir heller ikke noe mer tekst å lagre.


 */


//Registrer et nyfunnet ord i arrayet tekst, som holder hele innholdet
void tekstkompressor::reg_tekst(ord *o) {
	tekst[ant_ord++] = o;
}

// HS: -------------------
// Metode lagt til tekstkompressor som lager en liste over setningene. Første indeks har verdi 0, indeks 1 har verdi lik sluttposisjon (->til) for første setning i 'data', indeks 1 har sluttposisjon for setning 2
void tekstkompressor::reg_setn(int end) {
	//if (ant_setn < 100 || ant_setn > 619120) printf("Legger til setning nr. %i (%i - %i)\n", ant_setn+1, setninger[ant_setn],end);
	setninger[++ant_setn] = end;
}
// HS -------------------------------------------

void tekstkompressor::antallstats(int *liste, int ant, ordtre *tre) {
	ordlager_c *olager = tre->ordlager;
	int stats[500];
	int stats1k = 0;
	int stats10k = 0;
	int statsbig = 0;
	int tot = olager->les_antall();
	for (int i = 0; i < 500; ++i) stats[i] = 0;
	for (int i = 0; i < ant; ++i) {
		ord *o = olager->hent(liste[i]);
		if (o->antall < 500) ++stats[o->antall];
		else if (o->antall < 1000) ++stats1k;
		else if (o->antall < 10000) ++stats10k;
		else ++statsbig;
	}

	double count = 0.0; 

	for (int k = ant; k < 3; --k) {
		printf("%i\n", k);
		ord *o = olager->hent(liste[k]);
		count += o -> antall;
		if (count > 100 && count < 5000) {
			printf("Count: %f\n", count);
		}
	}

	printf("Antall ord med gitt lengde for %s\n", tre->navn);    

	for (int i=0; i<500; ++i) {
		// HS: Kommentert ut linje under midlertidig:
		 //printf("i:%3i    ant:%7i   %%: %2.1f   ‰tot: %2.1f\n", i, stats[i], 1000.0*stats[i]/tot, 100.0*i*stats[i]/tre->lestotal());
	}
	printf("  500-  999: %7i   %%:%2.1f\n", stats1k,  100.0*stats1k/tot);
	printf(" 1000- 9999: %7i   %%:%2.1f\n", stats10k, 100.0*stats10k/tot);
	printf("      flere: %7i   %%:%2.1f\n", statsbig, 100.0*statsbig/tot);
}

//Vellykket forsøk på å forbedre skriv1()
void tekstkompressor::skriv6(ordtre *tre, ordtre *btre) {
	if (!tre->lesantall()) return;
	int max_suffix = tre->lesantall() / MINSTESERIE;
	//Disse to indekseres med suffixnummer
	//Samme ord kan være base for flere suffixlengder.
	int suf_ord[max_suffix]; //ordnumre hvis endelse brukes som suffix
	int suf_len[max_suffix]; //lengder på suffixene, fra lengste til korteste
	int *liste = new int[tre->lesantall()];
	int *bliste = new int[btre ? btre->lesantall() : 0];
	int onr = 0;
	tre->rot->tre_til_liste(liste, &onr); //sortert ordliste
	if (verbose >= 6) antallstats(liste, onr, tre);
	finn_overlapp(onr, liste, tre);
	int lengste_suffix = 0;
	int ant_suffix = 0;
	if (btre) {
		onr = 0;
		btre->rot->tre_til_liste(bliste, &onr);
		finn_felles_endelser(onr, bliste, btre->ordlager);
		lengste_suffix = finn_lengste_suffix(onr, bliste, btre->ordlager);
		if (lengste_suffix < MINFIX) lengste_suffix = 0;
		printf("Lengste brukbare suffix: %i\n", lengste_suffix);

		//Finne suffix, til det ikke er mer å finne:
		for (int lengde = lengste_suffix;lengde >= MINFIX; --lengde) {
			finn_suffix3(lengde, &ant_suffix, onr, bliste, tre->ordlager, suf_ord, suf_len);
		}
	}	
	//Lag statistikker (spesielt for skriv6: *lengden* på suffixord)

	//Finn max overlapp og max endelse
	int max_overlapp = 0;
	int max_endelse = 0;
	int ant_bforskjeller = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		if (o->endelse > max_endelse) max_endelse = o->endelse;
		if (o->overlapp > max_overlapp) max_overlapp = o->overlapp;
	}

	listestat stat_overlapp(0, max_overlapp);
	listestat stat_bforskjell(1, 255);
	listestat stat_endelse(0, max_endelse);
	listestat stat_txt(1, 255);
	listestat stat_suff(0, ant_suffix);

	// HS: ----------------------  oppretter en ordfordeling (listestat) for treet med kapasitet lik antall unike ord.
	tre->ordfordeling = new listestat(0, tre->lesantall());
	// ----------------------------------------------------------------
	//txt nivå1:
	listestat stat_txt1[256] = { 
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},
		{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255},{1,255}
	};
	
	//Fyll opp statistikkene
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);

		// HS: ---------------------- 
		// HS: Legger inn antall forekomster av hvert ord i ordfordeling(listestat), bruker 'add' og ikke 'tell' for å legge inn en frekvens og ikke inkrementere med 1.
		tre->ordfordeling->add(liste[i], o->antall);
		// ----------------------------------------------------------------

		stat_overlapp.tell(o->overlapp);
		if (o->bokstavforskjell) {
			stat_bforskjell.tell(o->bokstavforskjell);
			++ant_bforskjeller;
		}
		stat_endelse.tell(o->endelse);
		int e = o->endelse;
		int j = o->fra + o->overlapp;
		if (o->bokstavforskjell) ++j;
		unsigned char forrige = data[j-1];
		if (e) stat_txt.tell(forrige); //Til hjelp for nivå1 	
		while (e--) {
			//Nivå 0
			if (e) stat_txt.tell(data[j]); //Ikke tell den siste, som ikke har etterfølgere
			//Nivå 1
			stat_txt1[forrige].tell(data[j]);
			forrige = data[j];
			++j;
		}
		stat_suff.tell(o->endelse2 + 1); 
	}

	//Skriv statistikker
	//suffixliste:
  //antall suffix, begr. av antallord / MINSTESERIE  (dropp resten hvis 0)
 	//lengste suffix, begr av lengste ord.
  //1.antall suffix med gitt lengde, begr av gjenv. suffix
  //2... ordnumre til disse suffixene, begr. av antall ord
  //3.red. i suffixlengde, begr av (forr.suffixlengde-MINFIX, ikke 0)
	//4 omigjen fra 1. til alle er kodet
	kod_tall(ant_suffix, 0, tre->lesantall() / MINSTESERIE);
	if (ant_suffix) {
		int gjenv_suffix = ant_suffix;
		lengste_suffix = suf_len[0]; //Korrigering, forrige var kanskje optimistisk
		kod_tall(lengste_suffix, MINFIX, max_ordlengde);
		int nr = 0;
		int dennelengden = lengste_suffix;
		while (gjenv_suffix) {
			//finn antall suffix med denne lengden
			int ant = 0;
			for (int i = nr; i < ant_suffix; ++i) {
				if (suf_len[i] == dennelengden) ++ant;
				else break;
			}
			kod_tall(ant, 1, gjenv_suffix);
			while (ant--) {
				kod_tall(suf_ord[nr], 0, tre->lesantall()); //suf_ord[nr]...
				++nr;
				--gjenv_suffix;
			}
			if (!gjenv_suffix) break;
			kod_tall(dennelengden-suf_len[nr], 1, dennelengden - MINFIX);
			dennelengden = suf_len[nr];
		}
	}
	stat_overlapp.akkumuler();
	stat_bforskjell.akkumuler();
	stat_endelse.akkumuler();
	stat_txt.akkumuler();
	for (int i=0; i <= 255; ++i) stat_txt1[i].akkumuler();
	stat_suff.akkumuler();
	//MINSTESERIE 8, MINFIX 3, enwik8: fil: 12129  (12k)
	kod_tall(max_overlapp, 0, max_ordlengde);
	kod_tall(max_endelse, 0, max_ordlengde);
	kod_tall(ant_bforskjeller, 0, tre->lesantall());
	stat_overlapp.til_fil(this, max_overlapp, tre->lesantall());
	stat_bforskjell.til_fil(this, 255, ant_bforskjeller);
	stat_endelse.til_fil(this, max_endelse, tre->lesantall());
	//fil: 12518
	if (ant_suffix) {
		kod_tall(stat_suff.fsum, 0, tre->lesantall());
		stat_suff.til_fil(this, ant_suffix, stat_suff.fsum);
	}
	//fil: 22727

	kod_tall(stat_txt.fsum, 0, tot_bytes);
	stat_txt.til_fil(this, 255, stat_txt.fsum);
	skrivtxtstat1(&stat_txt, stat_txt1); //skriver stat_txt også, m.mods
	//fil: 23168

	//Skriv ordlista v.hj.a. statistikker
	//spesielt for skriv6: 
	//1. Begr. lengden på overlapp m. forrige ords lengde, (alltid kjent)
	//2. Suffixord kan komprimeres hardere fordi lengden er kjent på forhånd
	ord *o = &nullord;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *forrige = o;
		o = tre->ordlager->hent(liste[i]);
		stat_overlapp.kod_grense(o->overlapp, forrige->lengde(), this);
		if (o->bokstavforskjell) stat_bforskjell.kod(o->bokstavforskjell, this);
		stat_endelse.kod(o->endelse, this);
		int j = o->fra + o->overlapp;
		if (o->bokstavforskjell) ++j;
		unsigned char forrige_tegn = data[j-1];
		for (int e = o->endelse; e; --e) {
			stat_txt1[forrige_tegn].kod(data[j], this);
			forrige_tegn = data[j];
			j++;
		}
		if (ant_suffix) {
			if (!o->bokstavforskjell && !o->endelse) {
				stat_suff.kod_nix0(o->endelse2+1, this);
			} else {
				stat_suff.kod(o->endelse2+1, this);
			}
		}
	}
	//Uten teksten: fil: 644310
	//Med txt: 1018426 Ny rekord, og det før tuning!
	//tegn:522193, bytes:374116 71%. Bedre med nivå1, est. 926000


	//Etter tuning: 990530 (MINSTESERIE 10, MINFIX 2)
	//tegn: 356564  est. 827000 m. nivå1
	printf("ant_suffix: %i\n", ant_suffix);
	printf("antall tegn: %i\n", stat_txt.fsum);
	delete[] bliste;
	delete[] liste;
}
/*
 Eksperiment II
 1. Finn optimale suffix, men ikke prefix. Lag nye suff-ord,->treet
 2. Når suffixene er lagd, bruk ordbokkompresjon på treet,
    uten å ødelegge suffix. 
		ordbokkompresjon kan bruke en var. del av ordet over, så man kan
		lettere få fler ord som bare har start og suffix og ingen 
		mellombokstaver.

		Fler ideer:  testet
		Når vi har en serie som er for kort til å reg. et suffix, 
		(1..MINSTESERIE-1) sjekk om vi tilfeldigvis har akkurat det 
		ordet i ordlista fra før. I så fall, bruk det som suffix likevel. 
		Det er gratis når vi ikke trenger lage nyord.
		Gjøres best i funksjonen som finner suffix

		Ide: lagre ordlengden i tillegg. Trenger bare velge mellom 
		suffix av riktig lengde. (mye færre). Koster bits et sted,
		sparer et annet sted. Komplisert å kode?
		Gjør det også mulig å vite når det ikke er noe suffix.
		Legger fler begrensninger på overlapp og endelse, korter
		kanskje ned disse. Bør undersøkes. (skriv4)


		antall like,bokstavforskjell,endelse, evt. suffix

		gml.system:
    test    
		testa    4like b.forskj.=0, ende 1
    testb    4like b.forskj.=1, ende 0
		testbest 5like b.forskjell=0, ende 3

    nytt: Hvis vi kopierer HELE forrige ord,
		hopper vi over bokstavforskjell. (koder ikke feltet, sparer bits)
		
		aldri b.forskjell=0 pga. forlenget ord.
		Kan likevel få b.forskjell=0, fordi vi kopierer et halvt ord
		og følger på med et suffix. forskjell=0 fordi ordet passer,
		men vi kopierer ikke mer, for å få plass til suffixet.
		Når vi kopierer mindre enn hele ordet, har vi alltid en bokstavforskjell

		Hvis endelse=0 når vi har kopiert hele ordet, får vi
		garantert et suffix (ellers er ikke ordet unikt)
		Men kan utpakker dekode dette? Utpakker vet ikke lengden på
		forrige ord, HVIS det har suffix. Men vet hvis det IKKE har
		suffix, så da kan infoen brukes...

		Hvis vi har bokstavforskjell=0, har vi derimot ingen endelse,
		og koder ikke endelsesfeltet. Og vi HAR et garantert suffix.

		test    
		testa    like=4,(ingen bforskjell) ende=1
		testc    like=4,b.forskjell=2,ende=0
		testcost like=5,(ingen forskjell) ende=3
		test|ing like=3,b.forskjell=0,(ingen ende), garantert suffix
		tett|ing like=2,b.forskjell=1,ende=1,suffix

		test
		test|ing like=4,(ingen bforskjell) ende=0, gar. suffix

		Ordbokkompresjon: 
		1. lengde som kopieres fra ordet over
		2. bokstavforskjell fra ordet over, (ofte 1)
		   
 */
void tekstkompressor::skriv3(ordtre *tre, ordtre *bakltre) {
	int ant_tekstord = tre->lesantall();
	printf("Antall ord i fil: %i\n", tre->lesantall());

	int liste[ant_tekstord + 2*ant_tekstord / MINSTESERIE];
	int onr = 0;
	bakltre->rot->tre_til_liste(liste, &onr);
	finn_felles_endelser(onr, liste, bakltre->ordlager);
	int lengste_suffix = finn_lengste_suffix(onr, liste, bakltre->ordlager);
	if (lengste_suffix < MINFIX) lengste_suffix = 0;
	printf("Lengste brukbare suffix: %i\n", lengste_suffix);
	//Finn felles suffix, lengste først:
	int ant_suffix = 0;
	for (int lengde = lengste_suffix; lengde >= MINFIX; --lengde) {
		bool nyeord = finn_suffix2(lengde, &ant_suffix, onr, liste, tre, bakltre);
		//Nye prefix/suffix-ord laget, så oppdater liste+bakliste:
		if (nyeord) {
			onr = 0;
			bakltre->rot->tre_til_liste(liste, &onr);
			finn_felles_endelser(onr, liste, bakltre->ordlager);
		}
	}
	//Felles suffix funnet, nye ord laget.
 	printf("Fant %i suffix. Har nå %i ord\n", ant_suffix, tre->lesantall());	
	//Ordbokkompresjon. Skaff en sortert liste først.
	onr = 0;
	tre->rot->tre_til_liste(liste, &onr);

	//Blir litt lavere enn antall ord, så tell for nøyaktig statistikk
	int ant_bforskjeller=0, ant_endelser=0;
	//Mer stats
	int max_endelse=0, max_overlapp=0;


	//Gå gjennom lista, finn felles begynnelse
	ord *o = &nullord;
	for (int i = 0; i < onr; ++i) {
		ord *forrige = o;
		o = tre->ordlager->hent(liste[i]);
		//Finn overlapp
		int of = o->fra;
		int ff = forrige->fra;
		int ot = o->fra + o->endelse - 1; //siste index som IKKE er i suffix, evt. siste i ord
		while (ff <= forrige->til && data[ff] == data[of] && of <= ot) { ++ff; ++of; }
		//ff og of er på første ulike, 
		//eller ff passerte slutten av forrige ord,
		//eller of gikk inn i suffix
		//(of kan ikke gå ut av ordet, da vil "ulike" slå til først.)
		if (ff > forrige->til) {
			//ff passerte forrige ord, vi kopierer hele ordet
			o->fix |= 2; //dropp bokstavforskjell
			o->bokstavforskjell = 0;
			o->overlapp = forrige->lengde();
			o->endelse -= o->overlapp;
			if (!o->endelse && forrige->endelse2 ==-1) o->fix |= 4;//garantert suffix, og utpakker er i stand til å oppdage det.
			++ant_endelser;
		}	else if (of <= ot) {
			//Må ha stoppet pga. av ulikhet, for stopp før suffix
			o->overlapp = of - o->fra;
			o->bokstavforskjell = data[of] - data[ff];
			o->endelse -= (o->overlapp + 1); //+1 for bokstavforskjellen
			++ant_endelser;
			++ant_bforskjeller;
		} else {
			//overlapp fortsatte inn i suffix. Lag perfekt overlapp
			o->overlapp = o->endelse - 1; //Siste håndteres med bokstavforskjell
			o->bokstavforskjell = 0;
			o->endelse = 0; //Kommer ikke til å kodes fordi bokstavforskjell==0
			o->fix |= 4; //Garantert suffix
			++ant_bforskjeller;
		}
		if (o->overlapp > max_overlapp)	max_overlapp = o->overlapp;
		if (o->endelse > max_endelse) max_endelse = o->endelse;
	} //slutt for-løkke

	//Har nå suffixer og overlapp+bokstavforskjell+endelse
  //Må lage statistikkene, og bruke dem til å kode ordlista
	//MINFIX3 17646 suffix, 11909 er nye ord. Resten, 5737, er gjenbrukte ord. (avst 85)
	//MINFIX2 18971         12042                     6929 (avst 70)

	//Merk ordene som er suffix, og bygg opp statistikk
	int max_bokst=0, sum_bokst=0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		o->nr = i;
		sum_bokst += o->endelse;
		if (o->endelse > max_bokst) max_bokst = o->endelse;
		if (o->endelse2 != -1) tre->ordlager->hent(o->endelse2)->fix |= 1;
	}

	listestat stat_overlapp(0, max_overlapp);
	listestat stat_bforskjell(0, 255);
	listestat stat_endelse(0, max_endelse);
	listestat stat_suff(0, ant_suffix);
	listestat stat_txt(1, 255);

	//Finn suffixordene i den sorterte rekkefølgen, og avstandene imellom
	//suffix 0: ikke suffix
	//suffix 1: første suffix i lista, osv
	int suffnr = 1;
	int maxavst = 0, nesteavst = 0;
	int forr = -1;
	int av1 = 0, av2_64 = 0, av65pluss = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		if (o->fix & 1) {
			o->suff_stat_ix = suffnr++;
			int avst = (i - forr);
			forr = i;
			if (avst==1) ++av1;
			else if (avst <= 64) ++av2_64; else ++av65pluss;
			if (avst > maxavst) {
				nesteavst = maxavst;
				maxavst = avst;
			} else if (avst > nesteavst) nesteavst = avst;
		} 
	}
	assert(suffnr < 65536); //Hvis ikke, utvid fra u.short til int.
	printf("Største suffix-avstand: %i, neststørste: %i\n", maxavst, nesteavst);
	printf("1: %3.1f%%  2..64: %3.1f%% 65+: %3.1f%%\n", 
			(float)av1/ant_suffix*100,
			(float)av2_64/ant_suffix*100,
			(float)av65pluss/ant_suffix*100);
//42% er 1. 53% u.64, trenger 6 bit. 5% trenger 14 bit
//95% under 64, trenger 6 bit. 5% trenger 14 bit.

	//Utpakker kjenner antall unike ord i kategorien, =ant_tekstord
	//Kod antall ekstra ord, så mottaker kan gjenskape totalen.
	int grense_nyord = ant_tekstord/MINSTESERIE*3; //Kan gjøres bedre, men...
	int nyord = tre->lesantall()-ant_tekstord;
	kod_tall(nyord, 0, grense_nyord); //Nye ord, altså suffix-ord
	
	kod_tall(maxavst, 1, ant_tekstord); //Max avst mellom suffixord blant de vanlige.
	kod_tall(nesteavst, 1, maxavst);    //Neststørste avstand mellom suffixord
	//suffix-stats
	kod_tall(av1, 0, ant_suffix);
	kod_tall(av2_64, 0, ant_suffix-av1);
	//av65pluss == ant_suffix-av1-av2_64, og trenger ikke overføres

	listestat fix(0, 2);
	fix.tab[0] = av1;
	fix.tab[1] = av2_64;
	fix.tab[2] = av65pluss;
	fix.akkumuler();

	//Overfør info om hvilke ord som er suffix. (suffix 0,1,2...)
	//Fyll også ut statistikkene som trengs
	forr = -1;

	int grense = maxavst;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		//Fyll ut statistikker:
		stat_overlapp.tell(o->overlapp);
		if (!(o->fix & 2)) stat_bforskjell.tell(o->bokstavforskjell);
		if ((o->fix & 2) || o->bokstavforskjell) stat_endelse.tell(o->endelse);
		stat_suff.tell( (o->endelse2 == -1) ? 0 : tre->ordlager->hent(o->endelse2)->suff_stat_ix);
		if (o->endelse) {
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.tell(data[start++]);
		}
		//Hvis ordet ER et suffix:
		if (o->fix & 1) {
			int avst = i-forr;
			if (avst == 1) fix.kod(0, this); //Trenger ikke kode avst, må være 1 her
			else if (avst <= 64) {
				fix.kod(1, this);
				kod_tall(avst, 2, 64);
			} else {
				fix.kod(2, this);
				kod_tall(avst, 65, grense);
				if (avst == maxavst) grense = nesteavst;
			}
			forr = i;
		}
	}

	//statistikkene er fyllt ut. Overfør dem, så utpakker kan bruke dem:
	kod_tall(max_overlapp, 0, max_ordlengde);
	kod_tall(max_endelse, 0, max_ordlengde);
	kod_tall(ant_endelser, 0, tre->lesantall());
	kod_tall(ant_bforskjeller, 0, tre->lesantall());
	stat_overlapp.akkumuler();
	stat_bforskjell.akkumuler();
	stat_endelse.akkumuler();
	stat_suff.akkumuler();
	stat_txt.akkumuler();
	stat_overlapp.til_fil(this, max_overlapp, tre->lesantall());
	stat_bforskjell.til_fil(this, 255, ant_bforskjeller);
	stat_endelse.til_fil(this, max_endelse, ant_endelser);
	stat_suff.til_fil(this, ant_suffix, tre->lesantall());
	//Antall tegn, begrenset av filstr: (Nede i 444807, fra 3mill?)
	kod_tall(stat_txt.fsum, 0, tot_bytes); //Begr. for stat_txt
	stat_txt.til_fil(this, 255, stat_txt.fsum);
	printf("Max overlapp: %3i  fsum: %i\n", max_overlapp,stat_overlapp.fsum);
	printf(" Max endelse: %3i  fsum: %i\n", max_endelse, stat_endelse.fsum);
	printf("ant_bforskjeller:%i ant_endelser:%i\n",ant_bforskjeller,ant_endelser);
	printf("bforskjeller.fsum %i\n", stat_bforskjell.fsum);
	printf("tot. suffix: %i\n",stat_suff.fsum);
	printf("txt.max_f %i, txt.fsum %i\n", stat_txt.max_f, stat_txt.fsum);



/*                                        Tillegg
	enwik8:        27 før alt dette 
	            12076 hoppavstander          +12049
              12134 stat overlapp          +   58
              12353 stat bokstavforskjell  +  219
							12456 stat endelser          +  103
              45906 stat suffix            +33450
							46349 stat txt               +  443

	skriv3-stats tar bare halvparten av skriv2-stats!	
Etter tuning: (6193 suffix,ned fra 18000)
                 20 før alt
               6046 hoppavstander
							 6467 stat overlapp+bforskjell+endelser
							18773 stat suffix
              19258 stat txt
							--------------
             219254 kodet overlapp             +199996
             425435 kodet bokstavforskjeller   +206181
						 574939 kodet endelser             +149504
						1289022 kodet tekst                +714083
						1559974 kodet suffix               +270952

Mengden tekst kan mer enn halvveres, men da tredobles antall
suffix og det spiser minst like mye plass.

*/	

	//Skriv selve ordlista, ved hjelp av statistikkene:
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		int suff = (o->endelse2 == -1) ? 0 : tre->ordlager->hent(o->endelse2)->suff_stat_ix;
		stat_overlapp.kod(o->overlapp, this);
		if (!(o->fix & 2)) stat_bforskjell.kod(o->bokstavforskjell, this);
		if ((o->fix & 2) || o->bokstavforskjell) {
			stat_endelse.kod(o->endelse, this);
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.kod(data[start++], this);
		}	
		if (o->fix & 4) stat_suff.kod_nix0(suff, this);
		else stat_suff.kod(suff, this);
	}

	/*prøve, u. garanterte suffix enwik8 ordliste: 1628729
	         m. garanterte suffix                  1626477 : -2252
	  Om vi bruker fler suffixmuligheter:    (2)   1656117
                                           (3)   1641854
                                           (4)   1636247
																					 (5)   1633003
																					 (6)   1630752
																					 (7)   1629397
																					 (8)   1628765
																					 (9)   1628363
																					(10)   1627369 :  +892
		Fler suffixmuligheter lønte seg ikke. 
		Konklusjon - å spare ord er ikke så lønnsomt som antatt.
	  Eller, MINSTESERIE er for lav som den er.
	     
		  MINSTESERIE 11
	    MINFIX størrelse
			     2   1626477
				   3   1616454
			     4   1591127
			     5   1569109 * klart minimum
					 6   1572755
					 7   1613893

			MINFIX 5 
 			MINSTESERIE størrelse
                7 1588964
								8 1581856
								9 1576349
							 10 1572179
							 11 1569216
							 12 1567409
							 13 1566045
							 14 1565232
							 15 1564753
							 16 1564284 * min
							 17 1564393
							 18 1564695
							 19 1564654
							 20 1564945

		 MINSTESERIE 16
		 MINFIX størrelse
		      3 1598168 
				  4 1574062
				  5 1564284 * min
				  6 1581872
				  7 1625560
				  8	1649243

			MINSTESERIE 16, MINFIX 5, opt. for skriv3()
MINSTESERIE2:
          15 1564157
			    14 1563772
					13 1563414
					12 1563074
					11 1562872
          10 1562520
				   9 1562272
				   8 1561748
				   7 1561585 * min
				   6 1561621 
				   5 1562296
				   4 1563800
				   3 1567255
				   2 1578310
Forbedring med skriv3():
MINSTESERIE 16
MINSTESERIE2 7
MINFIX 5

Etter at MINSTESERIE2 ble 7, øk MINSTESERIE videre:
	MINSTESERIE størrelse
	         16 1561585
					 17 1561280
					 18 1560981
					 19 1560679 * lok
					 20 1560738
           21 1560569  
					 22 1560109 
					 23 1560048  
					 24 1559974 * min
					 25 1560106
					 26 1560005 
					 27 1560588
Videre endring av MINSTESERIE2 ser ikke ut til å hjelpe. 
Optimalt for enwik8/skriv3():
MINSTESERIE 24
MINSTESERIE2 7
	    MINFIX 5

	*/

	/*txt ned fra 444k til 396k,   (2)
	  suff opp fra 18k til 40k,
	 tot.suff fra 503k til 501k !
   større avst. mellom suff - undertrykker de gode seriene?	 
  */
}

/*
  Som skriv4(), men:
	forutsetter sammenlign_shortlex() i stedet for sammenlign1()
  Treet sorteres i shortlex-orden, korteste ord først. Dermed,
	lett å overføre ordlengder uten å overføre masse data.
  Ulempe: færre ord som starter likt. Vi får se...
  1718713, som ikke er bra nok. (Før tuning).

Etter splitt: 1252302 færre (og kortere) unike ord fikk STOR effekt.
MINFIX str
     2 1262150
		 3 1252302
		 4 1252028 *
		 5 1277151
MINSTESERIE str
          8 1253294
					9 1252639
         10 1252028
				 11 1251754 
				 12 1251922
				 13 1251626
				 14 1251397 *
				 15 1252247
				 16 1252555
				 17 1253123
				 18 1254333
MINFIX str
     2 1257745
     3 1249304 *
     4 1251397
		 5 1283283
MINSTESERIE2 str
           2 1251397
					 3 1248492 *
					 4 1249052
					 5 1250198
MINSTESERIE str
         12 1249828
				 13 1249099
				 14 1248492 *
				 15 1248922
				 16 1248770
				 17 1248997
				 18 1249077
Optimalt for skriv5():
MINSTESERIE 14
MINSTESERIE2 3
MINFIX 3
				 */
void tekstkompressor::skriv5(ordtre *tre, ordtre *bakltre) {
	int ant_tekstord = tre->lesantall();
	printf("Antall ord i fil: %i\n", tre->lesantall());

	int liste[ant_tekstord + 2*ant_tekstord / MINSTESERIE];
	int onr = 0;
	
	bakltre->rot->tre_til_liste(liste, &onr);
	finn_felles_endelser(onr, liste, bakltre->ordlager);
	int lengste_suffix = finn_lengste_suffix(onr, liste, bakltre->ordlager);
	if (lengste_suffix < MINFIX) lengste_suffix = 0;
	printf("Lengste brukbare suffix: %i\n", lengste_suffix);
	//Finn felles suffix, lengste først:
	int ant_suffix = 0;
	for (int lengde = lengste_suffix; lengde >= MINFIX; --lengde) {
		bool nyeord = finn_suffix2(lengde, &ant_suffix, onr, liste, tre, bakltre);
		//Nye prefix/suffix-ord laget, så oppdater liste+bakliste:
		if (nyeord) {
			onr = 0;
			bakltre->rot->tre_til_liste(liste, &onr);
			finn_felles_endelser(onr, liste, bakltre->ordlager);
		}
	}

	//Felles suffix funnet, nye ord laget.
 	printf("Fant %i suffix. Har nå %i ord\n", ant_suffix, tre->lesantall());	
	//Ordbokkompresjon. Skaff en sortert liste først.
	onr = 0;
	tre->rot->tre_til_liste(liste, &onr);

	//Skriv info om hvordan ordlengdene endres (pos. for økning)
	//lengder øker fra 1 til max_ordlengde, altså max_ordlengde-1 endringer
	int forr_len = 1;
	int forr_pos = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
//For å skrive ordliste:
//skriv(o->fra,o->til);putchar('\n');
		//I teorien kan lengden øke med mer enn 1,men sjeldent:
		while (o->lengde() > forr_len) {
			kod_tall(i, forr_pos, tre->lesantall());
			forr_pos = i;
			++forr_len;
		}
	} //Heretter kan vi forutsette at ordlengdene er kjente. Pris:127 byte!

	//Blir litt lavere enn antall ord, så tell for nøyaktig statistikk
	int ant_bforskjeller=0, ant_endelser=0;
	//Mer stats
	int max_endelse=0, max_overlapp=0;


	//Gå gjennom lista, finn felles begynnelse
	ord *o = &nullord;
	for (int i = 0; i < onr; ++i) {
		ord *forrige = o;
		o = tre->ordlager->hent(liste[i]);
		//Finn overlapp
		int of = o->fra;
		int ff = forrige->fra;
		int ot = o->fra + o->endelse - 1; //siste index som IKKE er i suffix, evt. siste i ord
		while (ff <= forrige->til && data[ff] == data[of] && of <= ot) { ++ff; ++of; }
		//ff og of er på første ulike, 
		//eller ff passerte slutten av forrige ord,
		//eller of gikk inn i suffix
		//(of kan ikke gå ut av ordet, da vil "ulike" slå til først.)
		if (ff > forrige->til) {
			//ff passerte forrige ord, vi kopierer hele ordet
			o->fix |= 2; //dropp bokstavforskjell
			o->bokstavforskjell = 0;
			o->overlapp = forrige->lengde();
			o->endelse -= o->overlapp;
			if (!o->endelse) o->fix |= 4;//garantert suffix, og utpakker er i stand til å oppdage det.
			++ant_endelser;
		}	else if (of <= ot) {
			//Må ha stoppet pga. av ulikhet, for stopp før suffix
			o->overlapp = of - o->fra;
			o->bokstavforskjell = data[of] - data[ff];
			o->endelse -= (o->overlapp + 1); //+1 for bokstavforskjellen
			++ant_endelser;
			++ant_bforskjeller;
		} else {
			//overlapp fortsatte inn i suffix. Lag perfekt overlapp
			o->overlapp = o->endelse - 1; //Siste håndteres med bokstavforskjell
			o->bokstavforskjell = 0;
			o->endelse = 0; //Kommer ikke til å kodes fordi bokstavforskjell==0
			o->fix |= 4; //Garantert suffix
			++ant_bforskjeller;
		}
		if (o->overlapp > max_overlapp)	max_overlapp = o->overlapp;
		if (o->endelse > max_endelse) max_endelse = o->endelse;
	} //slutt for-løkke


	//Merk ordene som er suffix, og bygg opp statistikk
	int max_bokst=0, sum_bokst=0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		o->nr = i;
		sum_bokst += o->endelse;
		if (o->endelse > max_bokst) max_bokst = o->endelse;
		if (o->endelse2 != -1) tre->ordlager->hent(o->endelse2)->fix |= 1;
	}

	listestat stat_overlapp(0, max_overlapp);
	listestat stat_bforskjell(0, 255);
	listestat stat_endelse(0, max_endelse);
	listestat stat_txt(1, 255);

	//Finn suffixordene i den sorterte rekkefølgen, og avstandene imellom
	//suffix 0: ikke suffix
	//suffix 1: første suffix i lista, osv
	int suffnr = 0;
	int maxavst = 1, nesteavst = 1;
	int forr = -1;
	int av1 = 0, av2_64 = 0, av65pluss = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		if (o->fix & 1) {
			o->suff_stat_ix = suffnr++;
			int avst = (i - forr);
			forr = i;
			if (avst==1) ++av1;
			else if (avst <= 64) ++av2_64; else ++av65pluss;
			if (avst > maxavst) {
				nesteavst = maxavst;
				maxavst = avst;
			} else if (avst > nesteavst) nesteavst = avst;
		} 
	}
	assert(suffnr < 65536); //Hvis ikke, utvid fra u.short til int.
	printf("Største suffix-avstand: %i, neststørste: %i\n", maxavst, nesteavst);
	printf("1: %3.1f%%  2..64: %3.1f%% 65+: %3.1f%%\n", 
			(float)av1/ant_suffix*100,
			(float)av2_64/ant_suffix*100,
			(float)av65pluss/ant_suffix*100);
//42% er 1. 53% u.64, trenger 6 bit. 5% trenger 14 bit
//95% under 64, trenger 6 bit. 5% trenger 14 bit.

	//Utpakker kjenner antall unike ord i kategorien, =ant_tekstord
	//Kod antall ekstra ord, så mottaker kan gjenskape totalen.
	int grense_nyord = ant_tekstord/MINSTESERIE*3; //Kan gjøres bedre, men...
	int nyord = tre->lesantall()-ant_tekstord;
	kod_tall(nyord, 0, grense_nyord); //Nye ord, altså suffix-ord
	
	kod_tall(maxavst, 1, ant_tekstord); //Max avst mellom suffixord blant de vanlige. 
	kod_tall(nesteavst, 1, maxavst);    //Neststørste avstand mellom suffixord

	//suffix-stats
	kod_tall(av1, 0, ant_suffix);
	kod_tall(av2_64, 0, ant_suffix-av1);
	//av65pluss == ant_suffix-av1-av2_64, og trenger ikke overføres

	listestat fix(0, 2);
	fix.tab[0] = av1;
	fix.tab[1] = av2_64;
	fix.tab[2] = av65pluss;
	fix.akkumuler();

	//Overfør info om hvilke ord som er suffix. (suffix 0,1,2...)
	//Fyll også ut statistikkene som trengs
	listestat stat_sufff(0, ant_suffix); //samle-stat som overføres
	listestat *stat_suff[lengste_suffix+1]; //brukes til koding
	for (int i = MINFIX; i <= lengste_suffix; ++i) stat_suff[i] = new listestat(0, ant_suffix); //Sløser med plass, men disse skal ikke overføres...

	forr = -1;
	int grense = maxavst;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		//Fyll ut statistikker:
		stat_overlapp.tell(o->overlapp);
		if (!(o->fix & 2)) stat_bforskjell.tell(o->bokstavforskjell);
		//Alltid endelse hvis fix & 2
		//Ellers: endelse hvis bokstavforskjell !=0, OG
		//ordlengden ikke er brukt opp av overlapp+bokstav.
		//teller ikke endelser som ikke trenger overføres
		if ((o->fix & 2) || 
				(o->bokstavforskjell && (o->lengde() > o->overlapp+1))) stat_endelse.tell(o->endelse);
		//Vi (og mottaker) vet hvor mange tegn det er i suffixet.
		//Bruk derfor stat for suffix med bestemt lengde:
		if (o->endelse2 != -1) {
			ord *sf = tre->ordlager->hent(o->endelse2);
			stat_suff[sf->lengde()]->tell(sf->suff_stat_ix);
			stat_sufff.tell(sf->suff_stat_ix);
		}
		if (o->endelse) {
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.tell(data[start++]);
		}
		//Hvis ordet ER et suffix:
		if (o->fix & 1) {
			int avst = i-forr;
			if (avst == 1) fix.kod(0, this); //Trenger ikke kode avst, må være 1 her
			else if (avst <= 64) {
				fix.kod(1, this);
				kod_tall(avst, 2, 64);
			} else {
				fix.kod(2, this);
				kod_tall(avst, 65, grense);
				if (avst == maxavst) grense = nesteavst;
			}
			forr = i;
		}
	}

	//statistikkene er fyllt ut. Overfør dem, så utpakker kan bruke dem:
	kod_tall(max_overlapp, 0, max_ordlengde);
	kod_tall(max_endelse, 0, max_ordlengde);
	kod_tall(ant_endelser, 0, tre->lesantall());
	kod_tall(ant_bforskjeller, 0, tre->lesantall());
	stat_overlapp.akkumuler();
	stat_bforskjell.akkumuler();
	stat_endelse.akkumuler();
	stat_sufff.akkumuler();
	for (int i = MINFIX; i <= lengste_suffix; ++i) stat_suff[i]->akkumuler();
	stat_txt.akkumuler();
	stat_overlapp.til_fil(this, max_overlapp, tre->lesantall());
	stat_bforskjell.til_fil(this, 255, ant_bforskjeller);
	stat_endelse.til_fil(this, max_endelse, ant_endelser);
	kod_tall(stat_sufff.fsum, 0, tre->lesantall());
	stat_sufff.til_fil(this, ant_suffix, stat_sufff.fsum);
	//Antall tegn, begrenset av filstr: (Nede i 444807, fra 3mill?)
	kod_tall(stat_txt.fsum, 0, tot_bytes); //Begr. for stat_txt
	stat_txt.til_fil(this, 255, stat_txt.fsum);
	printf("Max overlapp: %3i  fsum: %i\n", max_overlapp,stat_overlapp.fsum);
	printf(" Max endelse: %3i  fsum: %i\n", max_endelse, stat_endelse.fsum);
	printf("ant_bforskjeller:%i ant_endelser:%i\n",ant_bforskjeller,ant_endelser);
	printf("bforskjeller.fsum %i\n", stat_bforskjell.fsum);
	printf("txt.max_f %i, txt.fsum %i\n", stat_txt.max_f, stat_txt.fsum);
	printf("tot.suffix: %i\n", stat_sufff.fsum);


	//Skriv ordlista, ved hjelp av statistikkene:
	int forrige_ordlengde = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		//overlapp begrenses av lengden på forrige ord,
		//og 1 under lengden på DETTE ordet.
		int grense = (forrige_ordlengde < o->lengde()-1) ? forrige_ordlengde : o->lengde()-1;
		forrige_ordlengde = o->lengde();
		stat_overlapp.kod_grense(o->overlapp, grense, this);
		if (!(o->fix & 2)) stat_bforskjell.kod(o->bokstavforskjell, this);
		//endelse begrenses av ordlengde-overlapp-bokst
		//og kodes ikke hvis vi grensa blir 0 eller vi VET
		//at et suffix bruker opp resten.
		//Alltid endelse hvis fix & 2
		//
		if ( (o->fix & 2) || 
				(o->bokstavforskjell && (o->lengde() > o->overlapp+1) ) ) {
			grense = o->lengde() - o->overlapp;
			if (o->bokstavforskjell) --grense;
			stat_endelse.kod_grense(o->endelse, grense, this);
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.kod(data[start++], this);
		}	
		//Lengden som er igjen, gir lengden på suffixet. Dropp helt hvis 0.
		grense = o->lengde() - o->overlapp - o->endelse;
		if (!(o->fix & 2)) --grense; //Bokstavforskjell brukes, også om den er 0
		//Sjekk at intet har gått galt:
//printf("ord %3i Grense:%2i ", i, grense);
//printf("end2: %6i ", o->endelse2);
//printf("sflen: %2i ", tre->ordlager->hent(o->endelse2)->lengde());
//printf("ovr:%2i bf: %2i, end:%2i ", o->overlapp, o->bokstavforskjell, o->endelse);
//skriv(o->fra, o->til);printf("\n");fflush(stdout);
		if (!grense) assert(o->endelse2 == -1);
		else assert(grense == tre->ordlager->hent(o->endelse2)->lengde());
		if (grense) {
			ord *sf = tre->ordlager->hent(o->endelse2);
			stat_suff[sf->lengde()]->kod(sf->suff_stat_ix, this);
		}
	}




/*                                        

skriv3 tunet, skriv4 IKKE tunet, men bruker skriv3-parametre.
Dermed: samme suffix osv, og forskjeller i kodeteknikk vises.
Tuning kan gjøre skriv4 bedre, antagelig med fler suffix 

   skriv3  skriv4                           tillegg3 tillegg4 spart
       20      11 før alt
			 --  251155 ordlengder                       0   251144 -251144
     6046  257190 hoppavstander                 6026     6035
		 6467  257876 stat overlapp+bforskj+end      421      686
		18773  264020 stat suffix                  12306     6144    6162
    19258  264505 stat txt                       485      485
   --------------
   219254  436464 kodet overlapp             +199996   171959   28037
   425435  642646 kodet bokstavforskjeller   +206181   206182 
	 574939  732145 kodet endelser             +149504    89499   60005
	1289022 1446228 kodet tekst                +714083   714083 
	1559974 1644013 kodet suffix               +270952   197785   73167

*/	
	for (int i = MINFIX; i <= lengste_suffix; ++i) {
		delete stat_suff[i];
	}
} //slutt skriv5()



/*
	Som skriv3(), men overfører lengdene på ordene først.
	Gir muligheter for å spare senere...
*/
void tekstkompressor::skriv4(ordtre *tre, ordtre *bakltre) {
	int ant_tekstord = tre->lesantall();
	printf("Antall ord i fil: %i\n", tre->lesantall());

	int liste[ant_tekstord + 2*ant_tekstord / MINSTESERIE];
	int onr = 0;
	bakltre->rot->tre_til_liste(liste, &onr);
	finn_felles_endelser(onr, liste, bakltre->ordlager);
	int lengste_suffix = finn_lengste_suffix(onr, liste, bakltre->ordlager);
	if (lengste_suffix < MINFIX) lengste_suffix = 0;
	printf("Lengste brukbare suffix: %i\n", lengste_suffix);
	//Finn felles suffix, lengste først:
	int ant_suffix = 0;
	for (int lengde = lengste_suffix; lengde >= MINFIX; --lengde) {
		bool nyeord = finn_suffix2(lengde, &ant_suffix, onr, liste, tre, bakltre);
		//Nye prefix/suffix-ord laget, så oppdater liste+bakliste:
		if (nyeord) {
			onr = 0;
			bakltre->rot->tre_til_liste(liste, &onr);
			finn_felles_endelser(onr, liste, bakltre->ordlager);
		}
	}
	//Felles suffix funnet, nye ord laget.
 	printf("Fant %i suffix. Har nå %i ord\n", ant_suffix, tre->lesantall());	
	//Ordbokkompresjon. Skaff en sortert liste først.
	onr = 0;
	tre->rot->tre_til_liste(liste, &onr);

	//Lag og skriv liste over ordlengder. Utpakker får vite ordlengder i forkant

	listestat stat_ordlengde(0, max_ordlengde*2);//!!!heller max for denne ordtypen (bokstavord/tallord/nonord)
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		stat_ordlengde.tell(o->lengde());
	}
	stat_ordlengde.akkumuler();
	stat_ordlengde.til_fil(this, max_ordlengde*2, tre->lesantall());
	/*
	ordliste:       var 11 byte
	stat_ordlengde:    138 
	ordlengder:     251155 
  forskjeller:    250962 sparte bare 193
	Så, 250kB! Kan det spares inn?
	sortere på lengde og deretter alfabetisk?
	*/
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		stat_ordlengde.kod(o->lengde(), this);
	}

	//Blir litt lavere enn antall ord, så tell for nøyaktig statistikk
	int ant_bforskjeller=0, ant_endelser=0;
	//Mer stats
	int max_endelse=0, max_overlapp=0;


	//Gå gjennom lista, finn felles begynnelse
	ord *o = &nullord;
	for (int i = 0; i < onr; ++i) {
		ord *forrige = o;
		o = tre->ordlager->hent(liste[i]);
		//Finn overlapp
		int of = o->fra;
		int ff = forrige->fra;
		int ot = o->fra + o->endelse - 1; //siste index som IKKE er i suffix, evt. siste i ord
		while (ff <= forrige->til && data[ff] == data[of] && of <= ot) { ++ff; ++of; }
		//ff og of er på første ulike, 
		//eller ff passerte slutten av forrige ord,
		//eller of gikk inn i suffix
		//(of kan ikke gå ut av ordet, da vil "ulike" slå til først.)
		if (ff > forrige->til) {
			//ff passerte forrige ord, vi kopierer hele ordet
			o->fix |= 2; //dropp bokstavforskjell
			o->bokstavforskjell = 0;
			o->overlapp = forrige->lengde();
			o->endelse -= o->overlapp;
			if (!o->endelse) o->fix |= 4;//garantert suffix, og utpakker er i stand til å oppdage det.
			++ant_endelser;
		}	else if (of <= ot) {
			//Må ha stoppet pga. av ulikhet, for stopp før suffix
			o->overlapp = of - o->fra;
			o->bokstavforskjell = data[of] - data[ff];
			o->endelse -= (o->overlapp + 1); //+1 for bokstavforskjellen
			++ant_endelser;
			++ant_bforskjeller;
		} else {
			//overlapp fortsatte inn i suffix. Lag perfekt overlapp
			o->overlapp = o->endelse - 1; //Siste håndteres med bokstavforskjell
			o->bokstavforskjell = 0;
			o->endelse = 0; //Kommer ikke til å kodes fordi bokstavforskjell==0
			o->fix |= 4; //Garantert suffix
			++ant_bforskjeller;
		}
		if (o->overlapp > max_overlapp)	max_overlapp = o->overlapp;
		if (o->endelse > max_endelse) max_endelse = o->endelse;
	} //slutt for-løkke


	//Merk ordene som er suffix, og bygg opp statistikk
	int max_bokst=0, sum_bokst=0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		o->nr = i;
		sum_bokst += o->endelse;
		if (o->endelse > max_bokst) max_bokst = o->endelse;
		if (o->endelse2 != -1) tre->ordlager->hent(o->endelse2)->fix |= 1;
	}

	listestat stat_overlapp(0, max_overlapp);
	listestat stat_bforskjell(0, 255);
	listestat stat_endelse(0, max_endelse);
	listestat stat_txt(1, 255);

	//Finn suffixordene i den sorterte rekkefølgen, og avstandene imellom
	//suffix 0: ikke suffix
	//suffix 1: første suffix i lista, osv
	int suffnr = 0;
	int maxavst = 1, nesteavst = 1;
	int forr = -1;
	int av1 = 0, av2_64 = 0, av65pluss = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		if (o->fix & 1) {
			o->suff_stat_ix = suffnr++;
			int avst = (i - forr);
			forr = i;
			if (avst==1) ++av1;
			else if (avst <= 64) ++av2_64; else ++av65pluss;
			if (avst > maxavst) {
				nesteavst = maxavst;
				maxavst = avst;
			} else if (avst > nesteavst) nesteavst = avst;
		} 
	}
	assert(suffnr < 65536); //Hvis ikke, utvid fra u.short til int.
	printf("Største suffix-avstand: %i, neststørste: %i\n", maxavst, nesteavst);
	printf("1: %3.1f%%  2..64: %3.1f%% 65+: %3.1f%%\n", 
			(float)av1/ant_suffix*100,
			(float)av2_64/ant_suffix*100,
			(float)av65pluss/ant_suffix*100);
//42% er 1. 53% u.64, trenger 6 bit. 5% trenger 14 bit
//95% under 64, trenger 6 bit. 5% trenger 14 bit.

	//Utpakker kjenner antall unike ord i kategorien, =ant_tekstord
	//Kod antall ekstra ord, så mottaker kan gjenskape totalen.
	int grense_nyord = ant_tekstord/MINSTESERIE*3; //Kan gjøres bedre, men...
	int nyord = tre->lesantall()-ant_tekstord;
	kod_tall(nyord, 0, grense_nyord); //Nye ord, altså suffix-ord
	
	kod_tall(maxavst, 1, ant_tekstord); //Max avst mellom suffixord blant de vanlige.
	kod_tall(nesteavst, 1, maxavst);    //Neststørste avstand mellom suffixord
	//suffix-stats
	kod_tall(av1, 0, ant_suffix);
	kod_tall(av2_64, 0, ant_suffix-av1);
	//av65pluss == ant_suffix-av1-av2_64, og trenger ikke overføres

	listestat fix(0, 2);
	fix.tab[0] = av1;
	fix.tab[1] = av2_64;
	fix.tab[2] = av65pluss;
	fix.akkumuler();

	//Overfør info om hvilke ord som er suffix. (suffix 0,1,2...)
	//Fyll også ut statistikkene som trengs
	listestat stat_sufff(0, ant_suffix); //samle-stat som overføres
	listestat *stat_suff[lengste_suffix+1]; //brukes til koding
	for (int i = MINFIX; i <= lengste_suffix; ++i) stat_suff[i] = new listestat(0, ant_suffix); //Sløser med plass, men disse skal ikke overføres...

	forr = -1;
	int grense = maxavst;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		//Fyll ut statistikker:
		stat_overlapp.tell(o->overlapp);
		if (!(o->fix & 2)) stat_bforskjell.tell(o->bokstavforskjell);
		//Alltid endelse hvis fix & 2
		//Ellers: endelse hvis bokstavforskjell !=0, OG
		//ordlengden ikke er brukt opp av overlapp+bokstav.
		//teller ikke endelser som ikke trenger overføres
		if ((o->fix & 2) || 
				(o->bokstavforskjell && (o->lengde() > o->overlapp+1))) stat_endelse.tell(o->endelse);
		//Vi (og mottaker) vet hvor mange tegn det er i suffixet.
		//Bruk derfor stat for suffix med bestemt lengde:
		if (o->endelse2 != -1) {
			ord *sf = tre->ordlager->hent(o->endelse2);
			stat_suff[sf->lengde()]->tell(sf->suff_stat_ix);
			stat_sufff.tell(sf->suff_stat_ix);
		}
		if (o->endelse) {
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.tell(data[start++]);
		}
		//Hvis ordet ER et suffix:
		if (o->fix & 1) {
			int avst = i-forr;
			if (avst == 1) fix.kod(0, this); //Trenger ikke kode avst, må være 1 her
			else if (avst <= 64) {
				fix.kod(1, this);
				kod_tall(avst, 2, 64);
			} else {
				fix.kod(2, this);
				kod_tall(avst, 65, grense);
				if (avst == maxavst) grense = nesteavst;
			}
			forr = i;
		}
	}

	//statistikkene er fyllt ut. Overfør dem, så utpakker kan bruke dem:
	kod_tall(max_overlapp, 0, max_ordlengde);
	kod_tall(max_endelse, 0, max_ordlengde);
	kod_tall(ant_endelser, 0, tre->lesantall());
	kod_tall(ant_bforskjeller, 0, tre->lesantall());
	stat_overlapp.akkumuler();
	stat_bforskjell.akkumuler();
	stat_endelse.akkumuler();
	stat_sufff.akkumuler();
	for (int i = MINFIX; i <= lengste_suffix; ++i) stat_suff[i]->akkumuler();
	stat_txt.akkumuler();
	stat_overlapp.til_fil(this, max_overlapp, tre->lesantall());
	stat_bforskjell.til_fil(this, 255, ant_bforskjeller);
	stat_endelse.til_fil(this, max_endelse, ant_endelser);
	kod_tall(stat_sufff.fsum, 0, tre->lesantall());
	stat_sufff.til_fil(this, ant_suffix, stat_sufff.fsum);
	//Antall tegn, begrenset av filstr: (Nede i 444807, fra 3mill?)
	kod_tall(stat_txt.fsum, 0, tot_bytes); //Begr. for stat_txt
	stat_txt.til_fil(this, 255, stat_txt.fsum);
	printf("Max overlapp: %3i  fsum: %i\n", max_overlapp,stat_overlapp.fsum);
	printf(" Max endelse: %3i  fsum: %i\n", max_endelse, stat_endelse.fsum);
	printf("ant_bforskjeller:%i ant_endelser:%i\n",ant_bforskjeller,ant_endelser);
	printf("bforskjeller.fsum %i\n", stat_bforskjell.fsum);
	printf("txt.max_f %i, txt.fsum %i\n", stat_txt.max_f, stat_txt.fsum);
	printf("tot.suffix: %i\n", stat_sufff.fsum);

	//Skriv ordlista, ved hjelp av statistikkene:
	int forrige_ordlengde = 0;
	for (int i = 0; i < tre->lesantall(); ++i) {
		ord *o = tre->ordlager->hent(liste[i]);
		//overlapp begrenses av lengden på forrige ord,
		//og 1 under lengden på DETTE ordet.
		int grense = (forrige_ordlengde < o->lengde()-1) ? forrige_ordlengde : o->lengde()-1;
		forrige_ordlengde = o->lengde();
		stat_overlapp.kod_grense(o->overlapp, grense, this);
		if (!(o->fix & 2)) stat_bforskjell.kod(o->bokstavforskjell, this);
		//endelse begrenses av ordlengde-overlapp-bokst
		//og kodes ikke hvis vi grensa blir 0 eller vi VET
		//at et suffix bruker opp resten.
		//Alltid endelse hvis fix & 2
		//
		if ( (o->fix & 2) || 
				(o->bokstavforskjell && (o->lengde() > o->overlapp+1) ) ) {
			grense = o->lengde() - o->overlapp;
			if (o->bokstavforskjell) --grense;
			stat_endelse.kod_grense(o->endelse, grense, this);
			int start = o->fra + o->overlapp;
			if (o->bokstavforskjell) ++start;
			for (int x = o->endelse; x; --x) stat_txt.kod(data[start++], this);
		}	
		//Lengden som er igjen, gir lengden på suffixet. Dropp helt hvis 0.
		grense = o->lengde() - o->overlapp - o->endelse;
		if (!(o->fix & 2)) --grense; //Bokstavforskjell brukes, også om den er 0
		//Sjekk at intet har gått galt:
//printf("ord %3i Grense:%2i ", i, grense);
//printf("end2: %6i ", o->endelse2);
//printf("sflen: %2i ", tre->ordlager->hent(o->endelse2)->lengde());
//printf("ovr:%2i bf: %2i, end:%2i ", o->overlapp, o->bokstavforskjell, o->endelse);
//skriv(o->fra, o->til);printf("\n");fflush(stdout);
		if (!grense) assert(o->endelse2 == -1);
		else assert(grense == tre->ordlager->hent(o->endelse2)->lengde());
		if (grense) {
			ord *sf = tre->ordlager->hent(o->endelse2);
			stat_suff[sf->lengde()]->kod(sf->suff_stat_ix, this);
		}
	}




/*                                        

skriv3 tunet, skriv4 IKKE tunet, men bruker skriv3-parametre.
Dermed: samme suffix osv, og forskjeller i kodeteknikk vises.
Tuning kan gjøre skriv4 bedre, antagelig med fler suffix 

   skriv3  skriv4                           tillegg3 tillegg4 spart
       20      11 før alt
			 --  251155 ordlengder                       0   251144 -251144
     6046  257190 hoppavstander                 6026     6035
		 6467  257876 stat overlapp+bforskj+end      421      686
		18773  264020 stat suffix                  12306     6144    6162
    19258  264505 stat txt                       485      485
   --------------
   219254  436464 kodet overlapp             +199996   171959   28037
   425435  642646 kodet bokstavforskjeller   +206181   206182 
	 574939  732145 kodet endelser             +149504    89499   60005
	1289022 1446228 kodet tekst                +714083   714083 
	1559974 1644013 kodet suffix               +270952   197785   73167

Tuning av skriv4(), før splitting
Utgangspunkt MINSTESERIE 24, MINSTESERIE2 7, MINFIX 5
MINFIX størrelse
     2  1653361
     3  1644184
     4  1634192 * 
		 5  1644013
		 6  1678472
skriv4 ser ut til å like kortere suffix bedre

MINFIX 4
MINSTESERIE størrelse
         40 1632479
         39 1632517
         38 1632057
         37 1632379
         36 1632469
         35 1631726 min
         34 1631922
         33 1631748 lok. min
         32 1632203
         31 1632642
         30 1633064
         29 1633329 
         28 1633644
         27 1633732
         26 1633765
				 25 1633958
         24 1634192
				 23 1634660
				 22 1635045
				 20 1636290
				 16 1639668

MINFIX 4, MINSTESERIE 35
MINSTESERIE2
           2 1635900
           3 1631369
           4 1630854 *
           5 1631038
           6 1630992 lok min
           7 1631726
           8 1632415
					 9 1633447
					10 1634134

Tuning etter splitting:
MINSTESERIE 14, MINSTESERIE2 3, 
MINFIX 
     2 1241784
     3 1230867
     4 1212160
     5 1198032
		 6 1195976
		 
Re-tuner ikke skriv4 nå. Venter til etter ordsplitting & case-folding
Men tallene her kan brukes for å se om casefolding etc. hjalp.
*/	
	for (int i = MINFIX; i <= lengste_suffix; ++i) {
		delete stat_suff[i];
	}
}

/*
	 ICU har funnet et «bokstavord». Sjekk om det trenger videre oppdeling,
	 og få ordet/ordene registrert

	* oppdelinger på :_.  (problem, kan skape tallord)
	  x_j  (index, holde dette sammen?)
  * 1.44MB er egentlig «tall med enhet» Burde vært «rent tall»+enhetsord
	  0,68kg  0.07780RT_c 0.86em 1.2beta hvorfor ikke tall allerede?
    ICU: bokstavord hvis det slutter på bokstav? 
		Mange som «025333411X»
		hva med 0000escapewin ? Kan forts. bli "tall+enhet"
		eller «01QuackeryRelatedTopics»
		eller «008new00a.ReftLocID» ? Bare rot? «new00a» er neppe en enhet?
	* forkortelser: bare store bokst
	* hex-tall som 0000AA ? (men kan ikke skilles fra BEEF o.l)
    «0203e8ba735348fd» hex. kan lagres kompakt. mixen passer ikke andre mønstre
		Klassifisere hex: HELE ordet (evt etter deling) må være hex, og 
		må inneholde sifre for å ikke tulle med ord som BEEF.
    hex sendes over til tallordene.
*/

/*
setning: (ord er både tall- og bokstavord) non er nonord, (space,.!?+;_)
ord non ord non ord non non non ord non non

ord: bokstavord, tallord eller sammensatt
sammensatt bokstavord:
  ord_ord_ord                 Splittes på _. KANSKJE unntak for xxx_i
	OrdOrdOrd                   Ren CamelCase kan deles opp
ord:ord.ord.ord_OrdOrd        Splittes på :._ unntak for forkortelser?
	20kg 40t                    00aa splittes i tall og rest/enhet
egne stats for sammensatte ord.

Sammensatte ord
Trinn 1: Først, oppdeling på :_  (og dropp unntak)
«Image:test_55.jpg» blir «Image»«:»«test»«_»«55.jpg»
«test_» blir «test»«_»
   Deretter forekommer ikke «:_» inni ord.
Trinn 2: enkel sjekk for tallord. En av:
         - ren hex, alså isxdigit på alle og minst en isdigit
				 - enten isdigit eller ispunct på ALT
				 behandles videre som tallord
				 - splitt «12.4km» til tallordet «12.4» fulgt av «km»
				   Serie med isdigit/ispunct fulgt av BARE isalpha.
Trinn 3:godta forkortelser a.aaaa a.a.aaa a.a.a.aaa osv
        Ellers, del opp på punktum. «this.is.slow»->«this»«.»«is»«.»«slow»
        Ny sjekk for tallord (ren hex eller digit+punct)
Trinn 4:Enklere sjekk for tallord. Bare sifre, eller sifre+hex. 
				Ellers, videre til trinn5
Trinn5:Splitt REN CamelCase. Stor fulgt av 1-fler små, Stor fulgt av...

Evt. casefolding må blir senere, når ALLE ord er funnet.



sammensatt tallord:
Trinn 1: splitt på _ men ikke på : eller .   alt ->trinn2
Trinn 2: - delord med isalpha hele veien, går inn på trinn5 bokstavord
				 - mønsteret aaa000 splittes i bokst.trinn5 og tallord, i håp om at
				   alpha-delen er et ord som ellers er i bruk.
				 - annet beholdes som de tallene eller raritetene de er.
Fra tallordene:
James145 Aaaaa000 
hextall godtas som tall
*/

/*
Tallord: 
 * rene tall som «45» «0082»
 * tall med punktum/komma i: «0,0,1,1» «0.0002» «0.4,0.4,0.95» «01.03.2002»
 * andre skilletegn: «0E:ED:B0» «0x0.4» «1'24.125»
 * tall med undersrek: «0,6115,1078809_1_0_»
 * andre tallsystemer: «೨೨»
 * noen hex-tall: «009a00», «00CC00» «0b7116abb4b7e3e9852560e5007688a0»
 * rariteter: «00W0» «00user001» «01T09» «01_2_page002» «100megsfree4»
   «1201_051201_archaeopteryx_2» «12ZTAX22» «950306_FRUS_XXIII_1961»
	 «996_intelligent_design_not_accep_9_10_2002»

 * ting som begynner på bokstav (mye):
   - også en del hex, «AF.E3.83.9B.E3.83»
   «A00000K3» «AB15» «AAJ_Autumn05_henry_16» «AAP1998» «AAlecture2»
   «AR2005120101798» «ARM7» «AS400»
	 «ATC_code_L01» «Aberdeenshire1911» «Abteilung5» «Act1»
	 «Afghan_Hound_600» «Africa1941» «Alpha2» «Amdahl1964»
	 «American_Airlines_Flight_77» «Americans_with_Disabilities_Act_of_1990»
	 «Amstrad_GX4000» «AnarchoCapitalism1» «Aston_Martin_DB9»

   «Example1» «Experiment123» «Explorer_35» «Exports_2005»
	 «Francs2000» «Franz_Kafka_1883» «FrenchSenate2007» «Friedrich14»
	 «Gulf_of_Finland_MER_FR_Orbit_07204_20030717»
	 «Hinduism_part_3» «HistSciTech000900240157»

Får mye bokstaver inn, ved at det egentlig er et ord (sted, produktnavn)
som følges av noen få sifre. 


Tall bør ihvertfall splittes på «_», og delene re-klassifiseres. (tall/bokst)
Vurdere å splitte i to deler slike som er bokstavord fulgt av et tall. Ofte er tallet et årstall som det nok fins fler av. Eller et kort produktnummer.

I setningskompresjonen er det kanskje ikke så viktig om noe er tall/bokstavord, men det ER viktig for ordbok-kompresjonen. 
	*/

//Trinn1 for bokstavord, kalles fra setningsoppdelingen
//deler på :_ og sender videre til trinn2
void tekstkompressor::reg_bokstavord1(int fra, int til) {
	int start = fra;
	for (int i = fra; i <= til; ++i) if (data[i] == ':' || data[i] == '_') {
		//Splitt fra første ord (hvis noe)
		if (start < i) reg_bokstavord2(start, i-1);
		nonordtre->nyord(i, i, NULL, 1);
		start = i + 1;
	}
	if (start <= til) reg_bokstavord2(start, til);
}

//Trinn2, skill ut visse tallord. Splitt «12,45kg» i tallord og bokstavord.
//Annet går til trinn3
//isxdigit på alt, og like antall sifre (hex-sifre forekommer i praksis parvis)
//isdigit/ispunct på alt -> tallord
//serie med isdigit/ispunct fulgt av isalpha deles opp i tall og enhet
//isalpha bør kunne inneholde punct, dele opp «007|DanielCraig.jpg»
void tekstkompressor::reg_bokstavord2(int fra, int til) {
	bool alt_hex = true;
	bool alt_num = true;
	bool num_fins = false;
	int alpha1;
	int tilstand = 0; //Tilstandsmaskin for å oppdage tall fulgt av ord

	//Oppdag hvis alt er hex, alt er numerisk, eller «45.2km»
	int length = til - fra + 1;
  for(int32_t i = fra; i <= til;) {
    UChar32 c;
		int forrige_i = i;
    U8_NEXT(data, i, length, c);
    alt_hex = alt_hex && u_isxdigit(c);

    alt_num = alt_num && (u_isdigit(c) || u_ispunct(c));
		num_fins = num_fins || u_isdigit(c); 
		switch (tilstand) {
			case -1: //Har gitt opp
				break;
			case 0:  //starttilstand
				tilstand = (u_isdigit(c) || u_ispunct(c)) ? 1 : -1;
				break;
			case 1: //i tall-delen
				if (u_isdigit(c) || u_ispunct(c)) break;
				if (u_isalpha(c)) {
					tilstand = 2;
					alpha1 = forrige_i;
				} else tilstand = -1;
				break;
			case 2: //i bokstavdelen
				if (!(u_isalpha(c) || u_ispunct(c))) tilstand = -1;
				break;
		}
  }
	if ((alt_hex && num_fins && !(length & 1)) || alt_num) {
		tallordtre->nyord(fra, til, bakltall, 1);
		return;
	}
	if (tilstand == 2) {
		//Tilfellet «1.3kg»
		tallordtre->nyord(fra, alpha1-1, bakltall, 1);
	} else alpha1 = fra;
	reg_bokstavord3(alpha1, til);
}

//Trinn3, godta forkortelser. Splitt andre konstruksjoner på «.»
//skill ut tallord som oppstår. (sifre/hex) Andre bokstavord til trinn4
void tekstkompressor::reg_bokstavord3(int fra, int til) {
	//Godta «C.S.Lewis», «V.S.O.P», «Å.Hansen»
	//Tilst: 0:venter alpha, evt. fulgt av dot. 
  //       1: dot eller alpha, 2: bare alpha
	//      -1: ugyldig som forkortelse
	int tilst = 0;
	bool pkt_fins = false;
	int length = til - fra + 1;
  for(int32_t i = fra; i <= til;) {
    UChar32 c;
    U8_NEXT(data, i, length, c);
		pkt_fins = pkt_fins || (c == '.');
		switch (tilst) {
			case 0:
				tilst = u_isalpha(c) ? 1 : -1;
			  break;
			case 1:
				if (c == '.') tilst = 0;
				else if (u_isalpha(c)) tilst = 2;
				else tilst = -1;
				break;
			case 2:
				tilst = u_isalpha(c) ? 2 : -1;
				break;	
		}
	}
	int start = fra;
	if (pkt_fins && (tilst == -1)) {
		//Del opp. Punktum fins, men ikke gyldig forkortelse
  	for(int32_t i = fra; i <= til;) {
			UChar32 c;
			int forrige_i = i; 
    	U8_NEXT(data, i, length, c);
			if (c == '.') {
				if (forrige_i > start) {
					reg_bokstavord4(start, forrige_i-1);
				}
				nonordtre->nyord(forrige_i, forrige_i, NULL, 1);
				start = i;
			}
		}
	} 	
	//Gyldig forkorting, eller ingen punktum. (Eller siste ord i oppdeling)
	if (start <= til) reg_bokstavord4(start, til);
}

//Trinn 4
//Skill ut rene tallord. isdigit på alt, eller isxdigit+partallengde+isdigit
void tekstkompressor::reg_bokstavord4(int fra, int til) {
	int length = til - fra + 1;
	bool num_fins = false;
	bool num_alt = true;
	bool hex_alt = true;
	for(int32_t i = fra; i <= til;) {
		UChar32 c;
   	U8_NEXT(data, i, length, c);
		num_fins = num_fins || u_isdigit(c);
		num_alt = num_alt && u_isdigit(c);
		hex_alt = hex_alt && u_isxdigit(c);
	}
	if (num_alt || (hex_alt && !(length & 1) && num_fins)) {
		tallordtre->nyord(fra, til, bakltall, 1);
		return;
	}
	reg_bokstavord5(fra, til);
}

//Trinn5: Splitt ren «CamelCase», godta alt annet som bokstavord.
//Tar ikke JamesBondDVDCollection pga flere store bokstaver på rad.
void tekstkompressor::reg_bokstavord5(int fra, int til) {
	//Sjekk om ordet er i CamelCase, altså inndelt i flere ord hvor
	//hvert starter med en stor og fortsetter med små. Slutt med liten bokst.
	//2 eller fler ord!
	int tilstand = 0;
	int antallstore = 0;
	//0: Forvent stor bokstav
	//1: Forvent liten bokstav
	//2: Forvent stor eller liten bokstav
	//-1: sporet av, (ikke ren CamelCase)
	int length = til - fra + 1;
	int delepkt[length];
  for(int32_t i = fra; i <= til;) {
    UChar32 c;
		int forrige_i = i;
    U8_NEXT(data, i, length, c);
		switch (tilstand) {
			case -1:
				break;
			case 0:
				tilstand = antallstore = u_isupper(c) ? 1 : -1;
				if (tilstand == 1) delepkt[0] = forrige_i;
				break;
			case 1:
				tilstand = u_islower(c) ? 2 : -1;
				break;
			case 2:
				if (u_islower(c)) break;
				if (u_isupper(c)) {
					delepkt[antallstore] = forrige_i;
					++antallstore;
					tilstand = 1;
				} else tilstand = -1;
				break;
		}
	}
	if (tilstand == 2 && antallstore > 1) {
		delepkt[antallstore] = til+1;
		for (int i = 0; i < antallstore; ++i) {
			bokstavordtre->nyord(delepkt[i], delepkt[i+1]-1, baklengstre, 1);
		}
	} else bokstavordtre->nyord(fra, til, baklengstre, 1);
}


//Trinn1 for tallord, kalles fra setningsanaysen
//Deler opp på _, overfører alt til trinn2
void tekstkompressor::reg_tallord1(int fra, int til) {
	int start = fra;
	for (int i = fra; i <= til; ++i) {
		if (data[i] == '_') {
			if (i > start) reg_tallord2(start, i-1);
			nonordtre->nyord(i, i, NULL, 1);
			start = i + 1;
		}
	}
	if (start <= til) reg_tallord2(start, til);
}

//Trinn 2 for tallord.
//Skill tall og tekst.
//rene alpha-ord -> bokstav trinn5
//del mønsteret aaa0000 i bokstavord og tallord
//alt annet (tall og rariteter) går som tallord
void tekstkompressor::reg_tallord2(int fra, int til) {
	int length = til - fra + 1;
	int tilstand = 0;
	int num1 = -1;
	bool allhex = true;
	bool allnum = true;
	//0: forvent alpha
	//1: bare alpha så langt
	//2: gått over på digit/punct
	//-1: sporet av (ikke aaa000)
	for(int32_t i = fra; i <= til;) {
		UChar32 c;
		int forrige_i = i;
   	U8_NEXT(data, i, length, c);
		allhex = allhex && u_isxdigit(c);
		allnum = allnum && u_isdigit(c);
		switch (tilstand) {
			case -1:
				break;
			case 0:
				tilstand = u_isalpha(c) ? 1 : -1;
				break;
			case 1:
				if (u_isalpha(c)) break;
				num1 = forrige_i;
				tilstand = (u_isxdigit(c) || u_ispunct(c)) ? 2 : -1;
				break;
			case 2:
				if (u_isxdigit(c) || u_ispunct(c)) break;
				tilstand = -1;
				break;
		}
	}
	//Ordet er klassifisert og evt. delt. Registrer bitene:
	if (allnum || (allhex && !(length & 1))) {
		tallordtre->nyord(fra, til, bakltall, 1);
		return;
	}
	switch (tilstand) {
		case -1: //Tallord, eller noe rart tidl. klassifisert som tall
			tallordtre->nyord(fra, til, bakltall, 1);
			break;
		case 1: //Hele ordet er alfabetisk
			bokstavordtre->nyord(fra, til, baklengstre, 1);
			break;
		case 2: //Ord som «James145», del opp
			bokstavordtre->nyord(fra, num1 - 1, baklengstre, 1);
			tallordtre->nyord(num1, til, bakltall, 1);
			break;
	}
}
/*
  Fungerende tegnklassifisering:
	skriv(fra,til);putchar('\t');
//'_:- ispunct
//150kg: 000aa
//CamelCase AaaaaAaaa
//Image:bad_4.JPG    Aaaaa.aaa.0.AAA
//upper/lower funker for latin, kyrillisk, gresk
//thai o.a. isalpha, men hverken upper eller lower
//isxdigit funker for all hex, både store og små bokst.
	int length = til-fra+1;
  for(int32_t i=fra; i<=til;) {
    UChar32 c;
    U8_NEXT(data, i, length, c);
    if (u_isxdigit(c)) putchar('x');
    else putchar('-');
  }
	putchar('\t');
  for(int32_t i=fra; i<=til;) {
    UChar32 c;
    U8_NEXT(data, i, length, c);
    if (u_isdigit(c)) putchar('0');
    else if (u_isupper(c)) putchar('A'); 
		else if (u_islower(c)) putchar('a');
		else if (u_isalpha(c)) printf("ª");
		else if (u_ispunct(c)) putchar('.');
		else putchar('?');
  }
  putchar('\n');

*/

void tekstkompressor::dump_tekst() {
	//Skriv tekst[] til fil, for å se at vi ikke har feil. 
	FILE *f = fopen("dump.txt", "w");
//	printf("DUMP\n");
	for (int i = 0; i < ant_ord; ++i) {
		fwrite(data+tekst[i]->fra, tekst[i]->lengde(), 1, f);
//		printf("ORD: %8i  ", i);skriv(tekst[i]->fra, tekst[i]->til);putchar('\n');
	}
	fclose(f);
}

/*
	Forsøk, sjekk størrelsen på statistikken for antall ord:
	1. alfabetisk rekkefølge
	2. shortlex rekkefølge, => høyfrekvente ord først => tettere grenser
	   => mindre forbruk

                FILSTØRRELSER    stat.str
	8.u. statistikk     1047303           0
	8.alfabetisk        2006952      959649 0.96M
  8.shortlex          1980466      933163 0.93M sparte  26486, 2.8%
	8.40stats           1742102      (uten ordlengdene)
	8.40stats+len       1742126      (bare 24 byte mer?)
	                                 694823 0.69M
	8.listestat         <ukj>



	9.u. statistikk     4824094           0
	9.alfabetisk        9369385     4545291 4.55M
  9.shortlex	        9253918     4429824 4.43M sparte 115467, 1.2%
	9.40stats+len       segfault - ikke debugget metode.
	9.listestat         <ukj>

Mulig fordeling som funksjon av lengde kan spare mer?
Eller de 100 største heller enn størst og neststørst?
En maxf og sumf for hver ordlengde? Litt ekstra info, mye spart?
I praksis en listestat for hver ordlengde.

Evt. liste alle single-ord v.hj.a. avstander? 6bits/ord stort sett?
e8 bruker i snitt 15 bits/ord for lengder.


Neste:
listestat med fordeling av ordantall. (så mange m. antall==1, 
så mange med antall==2 osv. Men ikke stats hele veien opp, 
regner med at stats får mange nuller på slutten. Så kod de
øverste på annet vis, så ikke STATS om ordantall blir så stort.

For å finne bra sluttpunkt for stat: se nærmere på hvordan fordelingen ser ut. Kutt stats når det er tilstrekkelig mange nuller på rad.



Trenger ordene sortert på antall.

*/
//#define LENGDER 40
void tekstkompressor::kod_tekst3() {
	//Tester kun ord (ikke tallord/nonord)
	int maxordnr = bokstavordtre->lesantall();
	ordtre *frektre = new ordtre("frekvens+shortlex bokstavord", &ordlager_b, &ordtre::sammenlign_shortlex, this);
	//fyll opp treet
	for (int i = 0; i < maxordnr; ++i) frektre->nyord3(i); 
	//finn ordene sortert på antall, det er det nyord3 gjør.
	int onr = 0;
	int liste[maxordnr];
	frektre->rot->tre_til_liste(liste, &onr);
	//sett nr-felter, og vis på skjerm
	int forrige_antall = 0;
	int like = 0;
	for (int i=0; i<maxordnr;++i) {
		ord *o = frektre->ordlager->hent(liste[i]);
		o->nr = i;

		//på skjermen:
		int antall = o->antall;
		if (antall != forrige_antall) {
			// HS: Kommentert ut linje under midlertidig:
			//printf("i:%6i o->antall:%6i     like:%6i   hopp:%i\n", i, forrige_antall, like, antall-forrige_antall);
			forrige_antall = antall;
			like = 0;
		} else ++like;
		/*
		Data:
          i: antall:  like:  hopp: 
      e8:
			      lave antall: mange like, og hopp 1. Passer for listestat
   403026 antall   1859: første hopp av lengde 10, like blir sjeldne
	 403546 antall   4500: tosifrede hopp er vanligst.
	 403746 antall  15000: tresifrede hopp vanligst.
	 403787 antall  30000: firesifrede hopp vanligst.
	 403807 antall  73139: første femsifra hopp. Bare 16 hopp til:
	 403823 antall 714911: «the» siste entry, max-ordet i enwik8
   
e8: listestat med 1859 entries. deretter 797 ord som kodes 
fra største og nedover:
<antall>(begr. av forrige antall) <ordnr> i ordliste
Deretter ordlengder for alle ordene i ordlista, i samme rekkef. så
ordnr ikke trenger å oppgis.
For ord med antall <= 1859: kod v.hj.a. listestat
For ord med antall >= 1859: escape797 og antallet begr. av maxant.

		e9:
		1638544 antall    4893: første hopp på lengde 10, like blir sjeldne
    1640118 antall   11336: tosifra hopp vanligst
		1641359 antall 4848530
		1641360 antall 6498163: maxordet i e9
    2816 ord etter første hopp på 10.

Gen. oppskrift:
   Finn første hopp på 10. Alt nedenfor kodes med listestat.
   For de over, lages en liste med antallene i synkende orden. Dermed
	 kan hvert antall være grenseverdi for det neste.
	 Ord-antall kan deretter kodes slik: rekkefølge som for ordlista.
	 små antall kodes med listestat. store antall kodes med en escape,
	 og deretter et nummer i lista over store tall.


	 strengt tatt har denne fordelingen et navn og en gitt form.
	 Kan kanskje bruke den matematisk, og bare kode avvik?  Men, mer 
	 å finne ut, og ikke så mye å vinne?
			 */


	}
	printf("antall:%6i     like:%6i\n", forrige_antall, like);

/* //Kode for 40stats+len
	//Finn startpkt for ulike lengder, og dermed også
	//hvor mange ord det er med hver lenge. 
	int nr0[LENGDER+2];

	int minstelengde = shortlextre->ordlager->hent(liste[0])->lengde(); //vanligvis 1
	int lengde = minstelengde;
	nr0[lengde] = 0;
	for (int i = lengde-1; i>=0; --i) {
		nr0[i] = -1; //Hvis minstelengde>1
	}
	//Fortsett
	int ii = 1;
	do {
		ord *o = shortlextre->ordlager->hent(liste[ii]);
		while (o->lengde() > lengde) {
			++lengde;
			nr0[lengde] = ii;
		}
		++ii; 
	} while (lengde < LENGDER && ii < maxordnr);
	nr0[41] = maxordnr;
	//NB!!! Hva om noen av ordlengdene er tomme, spesielt 40?
	//enwik8 er ikke slik, men kan skje i mindre filer.
	//Hvis dette skal brukes, må det være mer robust.


	//Har nå hvor i shortlex-rekkef. hver ordlengde starter.
	//antall med den gitte lengden er nr0[len+1]-nr0[len]

	//Alloker stats
	listestat *stat_ord[LENGDER+1];
	for (int i = 1; i<=LENGDER; ++i) {
		stat_ord[i] = new listestat(0, nr0[i+1]-nr0[i]);
	}
	//Opptelling
	for (int i = 0; i < maxordnr; ++i) {
		ord *o = shortlextre->ordlager->hent(liste[i]);
		int len = o->lengde();
		if (len > LENGDER) len=LENGDER;
		int o_stat_nr ;
		if (len < LENGDER) {
			o_stat_nr = i - nr0[len];
		} else {
			o_stat_nr = i - nr0[LENGDER];
		}
//if (dbg) printf("i:%6i   len:%3i, lengde:%3i		o_stat_nr:%i\n", i, len, o->lengde(),o_stat_nr);		
//if (len == 39) dbg=1;
//if (dbg) printf("len=%i\n",len);
		stat_ord[len]->add(o_stat_nr, o->antall);
	}


//Skriv alle stat_ord til output
	for (int i=1; i<=LENGDER; ++i) {
		stat_ord[i]->akkumuler();
		stat_ord[i]->til_fil(this, stat_ord[i]->max_i, stat_ord[i]->fsum);
	}

//Skriv lengdene ved hjelp av stat_ord
//	printf("maxordnr:%i\n",maxordnr);
  for (int i=0;i<maxordnr;++i) {
		ord *o= shortlextre->ordlager->hent(liste[i]);
		int len=o->lengde();
		if (len>LENGDER) len=LENGDER;
		int o_stat_nr;
		if (len < LENGDER) {
			o_stat_nr = i - nr0[len];
		} else {
			o_stat_nr = i - nr0[LENGDER];
		}
		stat_ord[len]->kod(o->antall,this);
//		printf("i=%5i    len=%2i    antall=%i\n",i,len,o->antall);
	}
//Slutt på forsøket 40stats+len
*/

/*

//	int liste[maxordnr];
//	bokstavordtre->rot->tre_til_liste(liste, &onr);
	for (int i = 0; i < onr; ++i) {
		ord *o = bokstavordtre->ordlager->hent(i);
		if (o->lengde() >= LENGDER) {
			ant_lengde[0]++; //0 for de >=LENGDER (ingen ord m. lengde 0)
		} else {
			ant_lengde[o->lengde()]++;
		}
	}
	bokstavordtre->merk(1,0);

	//opptelling
	listestat *stat_ord[LENGDER];
	for (int i = 1; i<LENGDER; ++i) stat_ord[i]


	listestat stat_ord(0, maxordnr);
	//Lag statistikken
	shortlextre->merk(1, 0);

	for (int i=0; i < ant_ord; ++i) {
		ord *o = tekst[i];
		if (o->typ) {
			//Bokstav-/tallord
			stat_ord.tell(o->nr);
		}
	}
	stat_ord.akkumuler();
	for (int i=0;i<maxordnr;++i) { //dbg
		printf("i:%7i  f:%7i : ", i, stat_ord.tab[i]);
		//Finn ordet
		for (int j=0;j<maxordnr;++j) {
			if (ordlager_b.ordtabell[j].nr == i) {
				skriv(ordlager_b.ordtabell[j].fra, ordlager_b.ordtabell[j].til);
				putchar('\n');
				break;
			}
		}
	}
	//Skriv statistikken
	stat_ord.til_fil(this, maxordnr, stat_ord.fsum);
	*/
}

/*
  Forsøk 2 på koding av tekst.
	1. skille mellom tag-basert tekst, og setningsbasert.
	   setninger fins mellom <text> setninger </text>
		 Evt. <comment> tekst </comment>
	2. Komprimere tags på egnet måte
	3. Sjekk om taggingen er feilfri. I så fall kan vi håndtere
	   <tagord annet> .... </tagord> og bare nevne «tagord» én gang!
		 Avsluttende tag kan i stedet være et fellestegn for slutt-tag.
		 I et feilfritt system, vil tag-slutt alltid avslutte den sist
		 åpnede tag'en, og det holder vi orden på med en stack.
		 Må også sjekke om bruken av < > er feilfri. Eller brukes < >
		 til annet enn tags?
	4. Egen nummerering for tag-ord. Egen nummerering for annet som
	   står inni tags, som key="4"
	5. noen tags har num. innhold: <id>5</id>
	6. Andre sammenh? <page> inneh. gjerne sekv. <title> <id> <revision>
	   <revision> inneh. <id><timestamp><contributor>
		 <contributor> inneh <username><id>

Fange opp allslags sammenh:
Stats for hva som fins inni <bestemttag>...</bestemttag>
Både hvilke tags, og rekkef? Ikke alltid the samme, contributor varierer.
Har tags alltid samme rekkef. når de er med?
stats for hvilke ORD som fins inni bestemte tags. <title> kan vel ha mye
rart, mens <username> er et begrensa sett?
En del std. <comment>Automated conversion</comment>

Ser ut til at <> bare brukes i tags?

tag sjekk: Putt hver «<» og «[» på en stack, sjekk når man finner >]
Sjekk evt. (){} også. Etter <tagord> putt «tagord» på stacken,
for å sjekke at tags matcher også.
Antall matcher eksakt for <> (enwik8, men 9)?
Matcher ikke perfekt for ()[]{}
*/
void tekstkompressor::kod_tekst2() {
	//Klargjøring
	nonordtre->merk(0, 0);
	bokstavordtre->merk(1, 0);
	tallordtre->merk(2, bokstavordtre->lesantall());

	ord *hake[2] = {nonordtre->tegnoppslag('<'), nonordtre->tegnoppslag('>') };
	ord *klamme[2] = {nonordtre->tegnoppslag('['), nonordtre->tegnoppslag(']')};
	ord *kroll[2] = {nonordtre->tegnoppslag('{'), nonordtre->tegnoppslag('}')};
	ord *par[2] = {nonordtre->tegnoppslag('('), nonordtre->tegnoppslag(')')};
	ord *slash = nonordtre->tegnoppslag('/');
	ord *space = nonordtre->tegnoppslag(' ');//!!!bør sjekke at vi får singlespace
	ordstakk os(1000);

	int tilstand = 0;
	for (int i=0; i < ant_ord; ++i) {
		ord *b;
		ord *tagord;

		//Tilstandsmaskin som finner og matcher tags
		switch (tilstand) {
			case 0: //Utenfor tag
				if (tekst[i] == hake[0]) tilstand = 1;
				break;
			case 1: //Har sett «<». Forventer «/» eller et ord
				if (tekst[i] == slash) { //Fant /
					tilstand = 10;
				} else { //Fant et ord?
					assert(tekst[i]->typ == 1);
					tagord = tekst[i];
					tilstand = 2;
				}
				break;
			case 2: //Har sett «<tagord», kan få inn div. annet, venter på «>» el. « />»
				if (tekst[i] == hake[1]) {
					os.push(tagord);
					tilstand = 0;
				} else if (tekst[i] == space) tilstand = 3;
				break;
			case 3: //Har sett «<tagord »  > eller /, alt annet ->2
				if (tekst[i] == hake[1]) {
					os.push(tagord);
					tilstand = 0;
				} else if (tekst[i] == slash) {
					tilstand = 4;
				} else if (tekst[i] != space) tilstand = 2; 
				break;
			case 4: //Har sett «<tagord /» Må ha >, eller gå til 2
				if (tekst[i] == hake[1]) tilstand = 0; else tilstand = 2;
				break;
			case 10: //Har sett «</». Forventer et ord
				assert(tekst[i]->typ == 1);
				tagord = tekst[i];
				tilstand = 11;
				break;
			case 11: //Har sett </tagord   Forventer «>»
				assert(tekst[i]==hake[1]);
				b = os.pop();
				if (b != tagord) {
					printf("Tag: "); 
					skriv(b->fra,b->til); 
					printf(" terminert med: ");
					skriv(tagord->fra,tagord->til);
					printf(" ???\n"); //Skjer én gang med enwik9, ikke med enwik8
					for (int j = i-400; j <= i; ++j) skriv(tekst[j]->fra,tekst[j]->til);
					putchar('\n');
				}
				tilstand = 0;
		}


		//if (tekst[i] == par[0]) os.push(par[1]);

		//if (tekst[i] == hake[1] && (b=os.pop()) != hake[1]) printf("Forventet %c, fikk %c\n", data[hake[1]->fra], b ? data[b->fra] : '-');
		//if (tekst[i] == par[1] && (b=os.pop()) != par[1]) printf("Forventet %c, fikk %c\n", data[par[1]->fra], b ? data[b->fra] : '-');

	}
	if (os.sp) {
		printf("Tags som ikke ble avsluttet:\n");
		ord *o;
		while ( (o = os.pop()) ) {
			skriv(o->fra, o->til);
			putchar('\n');
		}
	}
	if (os.pop() == NULL) printf("intet på stakk\n");
	printf("Test ferdig\n"); //tag-strukturen er PERFEKT, alle tags termineres korr.
	// < > matcher perfekt! De nøstes aldri heller.
	//Ikke []{}(), fordi de brukes i ascii-art og smileys o.l.
	//Noen måte å håndtere det? Resette telleverk/stacks ved linjeskift?
}

/*
	Forholdsvis enkel koding av teksten.
	Nivå1-statistikk for nonord (mellomrom, komma, ...)
	Nivå0-statistikk for ord+tallord
	Bruker ikke setningsinfo i det hele tatt.

	nonord har numre fra 0 og oppover.
	ord har numre fra 0 og opp, nummereringen fortsetter for tallord.

	Statistikken for nonord innfører ett nytt nonord: «generelt ord»
	Dermed, stat. for hva slags sep. som kommer etter et ord, 
	og sannsynligheten for å gå fra nonord til neste ord.
	Dekker også spesialtilfellet ordOrd hvor det ikke er nonord i mellom.

	Regner ikke med god kompresjon her, men det er en start. Får sett
	hvorvidt nonord-kompresjonen blir god.
*/
void tekstkompressor::kod_tekst1() {
	//Klargjøring
	nonordtre->merk(0, 0);
	bokstavordtre->merk(1, 0);
	tallordtre->merk(2, bokstavordtre->lesantall());

	int maxordnr = bokstavordtre->lesantall()+tallordtre->lesantall()-1;
	int maxsepnr = nonordtre->lesantall();//Denne indexen er spesiell, overg. til/fra ord
	listestat stat_ord(0, maxordnr);
	listestat stat_sep0(0, maxsepnr);
	listestat *stat_sep1[maxsepnr];
	for (int i = 0; i <= maxsepnr; ++i) stat_sep1[i] = new listestat(0, maxsepnr);

	//Lag statistikker
	int forrige_sep = maxsepnr;
	for (int i=0; i < ant_ord; ++i) {
		ord *o = tekst[i];
		if (o->typ) {
			//Bokstav-/tallord
			stat_ord.tell(o->nr);
			stat_sep1[forrige_sep]->tell(maxsepnr);
			forrige_sep = maxsepnr;
			stat_sep0.tell(maxsepnr);
		} else {
			//nonord, separatorer, ,.?:! " "...
			stat_sep0.tell(o->nr);//printf("forrige_sep:%i\n",forrige_sep);
assert(0 <= forrige_sep && forrige_sep <= maxsepnr);
			stat_sep1[forrige_sep]->tell(o->nr); //HVORFOR i blant kræsj m. invalid "this" i tell()?
			forrige_sep = o->nr;
		}
	}
	//Få regnskapet til å gå opp:
	stat_sep1[forrige_sep]->tell(maxsepnr);stat_sep0.tell(maxsepnr);
	//Skriv statistikkene, så utpakker kan bruke dem
	stat_ord.akkumuler();
	stat_sep0.akkumuler();
	for (int i=0; i<=maxsepnr; ++i) stat_sep1[i]->akkumuler();
	//Utpakker kjenner ordlistene, og dermed maxordnr
	printf("Totalt antall ord & tall: %i\n", stat_ord.fsum); //15078365
	kod_tall(stat_ord.fsum, maxordnr, tot_bytes);
	//fil: 1047303
	stat_ord.til_fil(this, maxordnr, stat_ord.fsum);
	//fil: 2171739  Trengte 1124436 til antallene. :-/

/*//vis noe av nivå1-stats, dbg
	for (int i=0; i<=maxsepnr; ++i) printf("%3i:   %8i   %8i   %8i   %8i\n", i, stat_sep0.tab[i], stat_sep1[maxsepnr]->tab[i],
		stat_sep1[1]->tab[i], stat_sep1[2]->tab[i]);//[2] største gr.
		*/
	//Utpakker har antall nonord, og dermed maxsepnr
	kod_tall(stat_sep0.fsum, maxsepnr, tot_bytes); 
	stat_sep0.til_fil(this, maxsepnr, stat_sep0.fsum);
	//fil: 2173278 (+1539)
	stat_sep0.ixsort();
	//Utpakker har stats på nivå0, det som trengs for å
	//utføre ixsort og pakke ut nivå1
	dbg=1;
	for (int i = 0; i <= maxsepnr; ++i) if (stat_sep0.tab[i]) stat_sep1[i]->til_fil_ix(this, stat_sep0.tab[i], &stat_sep0);
	//fil: 2180217 (+6939, lite for 512x1539 :-) 2M totalt så langt
	
	//Statistikker skrevet, bruk dem og skriv filinnholdet:
	forrige_sep = maxsepnr;
	for (int i=0; i < ant_ord; ++i) {
		ord *o = tekst[i];
		if (o->typ) {
			stat_sep1[forrige_sep]->kod(maxsepnr, this);
			stat_ord.kod(o->nr, this);
			forrige_sep = maxsepnr;
		} else {
			stat_sep1[forrige_sep]->kod(o->nr, this);
			forrige_sep = o->nr;
		}
	}
	//fil (bare sep):10202776  10M, sep=8M, den foran stat_ord 0.8M
	//fil (sep+ord): 32967326  32M, ord=20M
	/*
ord kan skvises ned MYE. (setning-statistikker, skjevfordelinger gjennom fila, nærhet til komma/kolon/o.l, inni tag-strukturer osv.

Det hjelper ikke, hvis sep har brukt opp mye av den plassen vi hadde.
Vel, målet er 14M, ikke 10M. Men likevel.

sep må skvises, det ser ikke like lett ut. En del kan ordnes fordi
sep inneholder <>[]() osv. Her kan vi regne med balanse. Har vi sett «<», får vi snart nok «>». Og etter «[[» kommer «]]». Men, må finne god modell.

Muligvis også: Etter <tag> kommer snart </tag>.
Kan søke opp matchende tag /tag, men hva med mismatcher? Fins de?

Å oppdage «<tagnavn>» evt. «</tagnavn>», fjerner <>/ fra "sep"
Mellomrom blir en større del av sep. 

Å oppdage par som «<tag> ... </tag>» gir mye mer struktur, og ordet «tag» kodes bare én gang selv om det står to ganger.

Andre strukturer:
«<tag ...ord="annetord">»
«<tag og mer fyllstoff>...</tag>»
innrykkene følger tag-strukturen
Noen tags har bare ett ord/tall, andre har fler nivåer med tags inni.
tekst inni tags, men ikke tags inni tags. Nøsting skjer mellom tags.
[[ ... noe ...]].  Her er nøsting mulig, altså [[...[[noe]]...]]
{{noe|noemer|...}}
[bare ett lag]

Noe tekst har tag-struktur, og noe er vanlig tekst pepret med [[ ]]
Vanlig tekst står forsåvidt inni <text>mange avsnitt</text>
text-tags kan være korte også.

Videre er det muligvis en rekkefølge for hvordan bestemte tags nøstes
inne i hverandre. Kan oppdages aut. med level1-stats for hvilke tags som står inni andre tags.
inni page fins title,id og revision. Inni revision, timestamp og contrib.

Noen tags har alltid numerisk innhold. F.eks. <id>24</id>. Kan oppdages, vi skiller på tallord. <timestamp>harogså et gitt format.
Ved å kode tag-strukturer, kanskje ikke så mange innmari lange setninger?

Bør muligvis oppdage tags først, og unicode-setninger bare mellom <text></text>? Veldig spesifikt for enwik/wikistoff da... Men når det starter med
en tag...
	*/
}

void tekstkompressor::pakk() {
	Locale loc = Locale(NULL); //Bruk kortreist oppsett
//	printf("Locale: %s\n", loc.getName());	
	//locale kan påvirke setningsoppdelingen. Det gjør ikke så mye,
	//for utpakkingen vil virke uansett. Å matche språket i fila kan hjelpe.
	
	//Filstørrelse
	struct stat st;
	stat(innfilnavn, &st);
	tot_bytes = st.st_size;
	//Fil->data
	data = new unsigned char[tot_bytes+1];
	fread(data, tot_bytes, 1, inn);
	nullord.fra = st.st_size; //Nullordet må starte med (char)0.
	nullord.til = st.st_size;
	nullord.antall = 0;
	nosep.fra = st.st_size;
	nosep.til = nosep.fra-1; //Så lengden blir 0.
	nosep.antall = 0;
	data[st.st_size] = 0; //Terminerende 0
	fclose(inn);
	//Konverter ascii/utf-8 til UnicodeString
	//UnicodeString ustring = UnicodeString::fromUTF8(data);
	UErrorCode status = U_ZERO_ERROR;

	UText *utext = utext_openUTF8(0, (char *)data, st.st_size, &status);

	if (!U_SUCCESS(status)) throw("UText feil");
	BreakIterator* setn_bi = BreakIterator::createSentenceInstance(loc, status);
	if (!U_SUCCESS(status)) {
		printf("feil:%i\n", status);
		throw("sentence feil");
	}
	BreakIterator* ord_bi = BreakIterator::createWordInstance(loc, status);
	if (!U_SUCCESS(status)) throw("word feil");
	BreakIterator* tegn_bi = BreakIterator::createCharacterInstance(loc, status);
	if (!U_SUCCESS(status)) throw("character feil");
	setn_bi->setText(utext, status);
	if (!U_SUCCESS(status)) throw("setn2 feil");
	ord_bi->setText(utext, status);
	if (!U_SUCCESS(status)) throw("ord2 feil");
	tegn_bi->setText(utext, status);
	if (!U_SUCCESS(status)) throw("tegn2 feil");

	int tot_ord = 0;
	int setn_start = 0;
	int setn_neste = setn_bi->following(setn_start);
	int setninger = 0;
	int setn_spm = 0;
	int setn_utrop = 0;
	int setn_punktum = 0;
	int setn_dbl_enter = 0;

#ifdef SKRIV5
	bokstavordtre = new ordtre("Bokstavord", &ordlager_b, &ordtre::sammenlign_shortlex, this);
#else
	bokstavordtre = new ordtre("Bokstavord", &ordlager_b, &ordtre::sammenlign1, this);
#endif
	baklengstre = new ordtre("Baklengs ordtre", &ordlager_b, &ordtre::sammenlign_bakl, this);
	nonordtre = new ordtre("Non-ord", &ordlager_n, this);
	bakltall = new ordtre("Baklengs talltre", &ordlager_t, &ordtre::sammenlign_bakl, this);
	tallordtre = new ordtre("Tallord", &ordlager_t, this);

	listestat st_setn_lengde(0, 21000); //Antall ord i setn

/*
Totalt 619158 setninger :-/
Enwik8 har «setninger» med over 6000 ord i. Og 2405 ordløse setn.
Men 1–30 er vanligst (over 10000 av hver)
1002 setninger med  61 ord
 120 setninger med 123 ord

 Ensifra antall setninger med 206 ord.
 Ingen hadde 319 ord – første null!
 Setninger med over 600 ord forekommer bare unntaksvis
 Den lengste setningen har 7364 ord.

1593                200
 882                255
 632                300
 342 setn. med over 400 ord
 191                500
	 */


	//OPPDELING I SETNINGER, V.HJ.A ICU
	while (setn_neste != BreakIterator::DONE) {
		++setninger;
		//Finn ut hvor setningen egentlig slutter. Et linjeskift avslutter ikke en setning,
		//men et dobbelt linjeskift gjør.
		//En setn. kan inneholde mange linjeskift.
		while (setn_bi->getRuleStatus() ==  UBRK_SENTENCE_SEP) {
			int s_pos = setn_bi->current();
			//Setn. avsluttet med linjeskift e.l. 
			//Prøv å gå videre, et enslig linjeskift avslutter ikke en setning.
			//To avslutter, fordi da er avsnittet slutt.
			//Å gå videre kan treffe strengslutten, i så fall backtrack:
			setn_neste = setn_bi->next();
			if (setn_neste == BreakIterator::DONE) {
				//Traff på EOF
				setn_neste = s_pos; //Tilbake til forrige
				break;
			}
			if (setn_neste == s_pos + 1) {
				++setn_dbl_enter;
				break; //Oppdaget dobbelt linjeskift.
			}
		}

		int ordnr = 1;
		int ord_start = setn_start;
		int ord_neste = ord_bi->following(ord_start);
		if (ord_neste > setn_neste) ord_neste = setn_neste; //icu-bugg, ord gjennom setningsskille.
		
		// HS -----------------------------
		// HS: Den siste setningen registreres bare med lengde 1 og "R" når den egentlig skal være:
		//   "Recently, Standard Japanese has " 
		// - jeg legger kun til start på neste setning for å definere ende på forrige, får grensetilfelle for siste setning... 
		reg_setn(setn_neste);  // HS Legger til setningens endepunkt i listen over setninger som tilhører 'tekstkompressor'
		// HS ----------------------------
		
		if (verbose >= 8) {
			printf("\nSetning nr %i:     (fra:%i til:%i)\n", setninger, setn_start, setn_neste);
			skriv(setn_start, tegn_bi->preceding(setn_neste));
			if (verbose >= 9) {
				printf("\nOppdeling i ord:\n");
			}
		}
		char setn_slutt = 0;
		int ord_i_setningen = 0;

		//Løkke som finner ordene i en setning
		do {
			int ord_slutt = ord_neste - 1;//tegn_bi->preceding(ord_neste);  //trenger SLUTTEN på siste unicode-tegn, ikke STARTEN på siste unicode-tegn. (UTF( unicode-tegn har flere bytes)
			//gjelder når ord_slutt brukes for å kopiere bytes.
			//ord_slutt kan IKKE brukes til andre formål. Spesielt ikke ICU,
			//som forv. STARTEN på unicode-tegn
			if (verbose >= 9) {
				printf("\nordnr %2i  (%2i-%2i): ", ordnr++, ord_start, ord_neste);
				skriv(ord_start, ord_slutt); //ok
				int lengde = ord_slutt - ord_start; //ok
				if (lengde < 20) for (int i = 20-lengde; i--; ) putchar(' ');
			}
			print_ord_status(ord_bi, verbose);

			switch (ord_bi->getRuleStatus()) {
				case UBRK_WORD_NONE: //Non-ord, som tegnsetting og mellomrom
					switch (data[ord_slutt]) {
						case '!':
						case '?':
						case '.':
							setn_slutt = data[ord_slutt]; //virker ok
							break;
					}
					//Bryt opp «>-» i «>»«-»  («>» fulgt av unicode soft hypen c2 ad)
					//Ødelegger en tag i enwik9, hvis vi ikke fikser det her
					//Bug i libicu???
					if (ord_slutt-ord_start == 2 && data[ord_start] == '>' && data[ord_start+1] == 0xc2 && data[ord_slutt] == 0xad) {
						//Myk bindestrek foran <
						nonordtre->nyord(ord_start, ord_start, NULL, 1);
						nonordtre->nyord(ord_slutt - 1, ord_slutt, NULL, 1);
					} else nonordtre->nyord(ord_start, ord_slutt, NULL, 1);
					break;
				case UBRK_WORD_NUMBER: //Sifre og ord med sifre inni
					reg_tallord1(ord_start, ord_slutt);
					//tallordtre->nyord(ord_start, ord_slutt, NULL, 1);
					++ord_i_setningen;
					break;
				case UBRK_WORD_LETTER: //vanlige bokstavord...
				case UBRK_WORD_IDEO:  //...ideogrammer er også ord...
				case UBRK_WORD_KANA: //...og kana
					//Videre arbeid med bokstavord:
					reg_bokstavord1(ord_start, ord_slutt);
					//bokstavordtre->nyord(ord_start, ord_slutt, baklengstre, 1);
					++ord_i_setningen; //Teller som et sammensatt ord
					// ordnr++;    //HS IKKE I BRUK lengre
					break;
				default:
					feil("Ukjent ordtype som ikke håndteres.");
					break;
			}
/*
Idiotproblem: En SETNING avsluttes I MIDTEN av et ord?
                                            v setning termineres der!
'''Achille-Claude Debussy''' ({{IPA|/ˌdɛ.buː.ˈsiː/}}) 1964.
^^^^      ^^     ^^      ^^^^^^^^  ^^^           ^
                                     ? "ordet" som starter her,
																		   går gjennom setningsslutt.
Enten kuttes ordet fordi det går ut over setningen sin.
Eller så får vi et halv-ord for mye, fordi neste setning starter
i midten av et ord som er lagret.

Mulig løsning: Når et ord strekker seg inn i neste setning,
KUTT det foran starten på neste setn. Huff. icu-bug?

Mulig bedre løsning: undertrykk feil setningsslutt!!!
*/	 

			ord_start = ord_neste;
			ord_neste = ord_bi->next();
			//Fix icu-bug:
			//Hvis siste ord strekker seg gjennom setningsskillet, kapp det!
			if (ord_start < setn_neste && ord_neste > setn_neste) ord_neste = setn_neste;
			++tot_ord;
		} while ( (ord_neste <= setn_neste) && (ord_neste != BreakIterator::DONE));
		if (verbose >= 8) printf("\nDet var %i ord i setningen.\n", ord_i_setningen);
		st_setn_lengde.tell(ord_i_setningen);

		setn_start = setn_neste;
		if (verbose >= 8) print_setn_status(setn_bi);
		if (setn_bi->getRuleStatus() == UBRK_SENTENCE_TERM) {
			if (verbose >= 9) printf("sym:%c\n", setn_slutt);
			switch (setn_slutt) {
				case '.': ++setn_punktum;break;
				case '!': ++setn_utrop;break;
				case '?': ++setn_spm;break;
			}
		}
		setn_neste = setn_bi->next();
	}   // HS: Bryting i setninger. ytre løkke



	printf("\nSetninger: %i   ord: %i\n", setninger, tot_ord);

	printf("Kategori    %9s   %9s   %9s   %9s   %9s\n", "totalt", "unike", "reps", "maxreps", "single");
	printf("Bokstavord: %9i   %9i   %9.1f   %9i   %9i\n", bokstavordtre->lestotal(), bokstavordtre->lesantall(), 
			bokstavordtre->snittreps(), bokstavordtre->lesmaxrep(), bokstavordtre->lessingle());
	printf("   Tallord: %9i   %9i   %9.1f   %9i   %9i\n",  tallordtre->lestotal(), tallordtre->lesantall(), 
			tallordtre->snittreps(), tallordtre->lesmaxrep(), tallordtre->lessingle());
	printf("    nonord: %9i   %9i   %9.1f   %9i   %9i\n", nonordtre->lestotal(), nonordtre->lesantall(), 
			nonordtre->snittreps(), nonordtre->lesmaxrep(), nonordtre->lessingle()); 

	printf("\n  ord/setn: %9.1f\n\n", (float)bokstavordtre->lestotal() / setninger);

	printf("Setninger avsluttet med:\n");
	printf(".    %7i\n", setn_punktum);
	printf("?    %7i\n", setn_spm);
	printf("!    %7i\n", setn_utrop);
	printf("lflf %7i\n", setn_dbl_enter);
  printf("------------------------\n");

	st_setn_lengde.akkumuler();
//	st_setn_lengde.dump();

	if (verbose >= 7) {
		bokstavordtre->skrivordliste();
		tallordtre->skrivordliste();
		nonordtre->skrivordliste();
	}
	//Vis baklengslista:
	//baklengstre->rot->skriv_inorden(NULL);
	//tallordtre->rot->skriv_inorden(NULL);
	ant_unike_ord = bokstavordtre->lesantall() + tallordtre->lesantall() + nonordtre->lesantall();
	if (verbose >= 1) {
		printf("%10s %9s\n", "Ordtype", "Maxlengde");
		printf("%10s %9i\n", "Bokstavord", bokstavordtre->lesmaxlengde());
		printf("%10s %9i\n", "Tallord", tallordtre->lesmaxlengde());
		printf("%10s %9i\n", "Nonord", nonordtre->lesmaxlengde());
	}
	max_ordlengde = bokstavordtre->lesmaxlengde();
	int x = tallordtre->lesmaxlengde();
	if (x > max_ordlengde) max_ordlengde = x;
	x = nonordtre->lesmaxlengde();
	if (x > max_ordlengde) max_ordlengde = x;
	unsigned short xx = max_ordlengde;
	if (verbose >= 1) printf("max ordlengde: %i\n", xx);

	//Skriv en basis, grenseverdier for aritmetisk koding
	fwrite(&ant_unike_ord, sizeof(int), 1, ut);
	fwrite(&xx, sizeof(short), 1, ut);//51648 med enwik9 - hva driver de med?

	//Begynn med aritmetisk koding:
	kod_tall(bokstavordtre->lesantall(), 0, ant_unike_ord);
	kod_tall(tallordtre->lesantall(), 0, ant_unike_ord - bokstavordtre->lesantall());
	//nonordtre->antall == ant_unike_ord-bokstavordtre->antall-tallordtre->antall
	//skrives ikke, fordi utpakker kan beregne det selv.
#ifdef SKRIV3
  skriv3(bokstavordtre, baklengstre); //Eksperiment II
#endif
#ifdef SKRIV4
	skriv4(bokstavordtre, baklengstre);
#endif
#ifdef SKRIV5
	skriv5(bokstavordtre, baklengstre);
#endif
#ifdef SKRIV6
	skriv6(bokstavordtre, baklengstre);
#endif

	skriv6(tallordtre, bakltall); //1:1119229 6.0:1128426 6.6:1119330
	skriv6(nonordtre, NULL);



// =======================================================
	//HS:  I bruk - setter ordenes type. Bruker ikke 'merk'-metoden, men husker ikke hvorfor. 

	for (int p = 0; p < bokstavordtre->ordlager->les_antall(); ++p) {
		ord *o = bokstavordtre->ordlager->hent(p);
		o->typ = 1;
		if (p < 100) {
			//skriv(o->fra, o->til);
			//printf("   (p: %i) \n", p);
		}
	}  // For ordene i 'tekst' er bare pointere til de ulike trærene sine ordlagre, ikke sant?

	for (int p = 0; p < tallordtre->ordlager->les_antall(); ++p) {
		ord *o = tallordtre->ordlager->hent(p);
		o->typ = 2;
	}  

	for (int p = 0; p < nonordtre->ordlager->les_antall(); ++p) {
		ord *o = nonordtre->ordlager->hent(p);
		o->typ = 0;
	} 


  // HS ============================================
	// I BRUK: 
		hs_stat2(); 

	//} 
// =====================================================


	printf("I teksten er det %i ord+tall+nonord.\n", ant_ord);
	printf("%8i bokstavord\n", bokstavordtre->lesantall());
	printf("%8i tallord\n", tallordtre->lesantall());
	printf("%8i non-ord\n", nonordtre->lesantall());

	//dump_tekst(); //Sjekk med diff, at fila er korrekt representert. OK
	//kod_tekst1();
	kod_tekst3();

	kod_avslutt();
	fclose(ut);
	delete utext;
}
/*
skriv1():   1715522  1.7MB, og litt bedre enn skriv3
     
skriv2():                   2.6MB, mislykka
skriv3():          1726721  1.7MB

	 */


/*
======
Filformat input:
---
Tekst, inndelt i ord og setninger.  (alle skriftspråk og kildekodespråk)
---
======
Filformat output:
---
antall unike ord: int
lengste ord: short (trengs mer, er ikke dette en TEKSTFIL)
#Deretter, aritmetisk komprimert fil:
antall unike bokstavord, begr. av unike ord
antall unike tallord, begr. av gjenværende unike ord
 (gjenværende ord er nå unike non-ord)
komprimert ordliste bokstavord
komprimert ordliste tallord
komprimert ordliste nonord
(komprimert filinnhold, som benytter ordlistene.)
---
======
Format komprimert ordliste
---
lengste ord i lista;             1..lengste ord

#stats for overlapping. sum av stats=antall unike ord
lengste overlapp (ant. entries); 0..lengste ord i lista   
#er også antall entries. Hvis 0, dropp neste felt
høyeste entry                 min: unike ord/ant. entries rundet opp, max: unike ord
#entries:
antall min:0, eller gjenv. for siste entry.  max:høyeste entry, eller gjenv.

#stats for byte i ordendelse sum av stats=antall unike ord i lista
lengste endelse (ant. entries)  1..lengste ord
høyeste entry                 min: unike ord/ant. entries rundet opp, max: unike ord
#entries, start på[1]:
antall 0 (siste:gjenværende)..høyeste entry (eller gjenværende)

#stats for bokstavforskjeller, sum 1 under antall unike ord
antall entries/største forskjell 1..255
høyeste entry										unike ord/ant.entries rundet opp..(unike ord)
#entries, start på [1]:
0..høyeste entry(eller gjenværende)
#Nivå0-modell for tegnene i endelsene. Antall tegn=sum for (i-1)*endelsesbyte[i]
ant. entries, 0..255
høyeste entry (antall tegn/ant. entries rundet opp)..antall tegn
#entries, start på [1] fordi tekst ikke har ascii0
0 (siste:gjenv.)..høyeste entry eller gjenværende
###Så kommer de unike ordene, kodet v.hj.a. statistikkene over:
antall overlappende tegn med forrige ord, fra fordeling
  men utvalg begrenset av lengden på forrige ord.
antall tegn i endelse, fra fordeling
bokstavforskjell for første tegn, fra fordeling
resterende tegn i endelsen, fra nivå0-modell 
---

 */

/*
Setning nr 3:     (fra:25 til:62)
«Dette er den andre setningen, av to.
»
Oppdeling i ord:

ordnr  1  (25-30): «Dette»                	bokstav-ord
ordnr  2  (30-31): « »                    	nonword
ordnr  3  (31-33): «er»                   	bokstav-ord
ordnr  4  (33-34): « »                    	nonword
ordnr  5  (34-37): «den»                  	bokstav-ord
ordnr  6  (37-38): « »                    	nonword
ordnr  7  (38-43): «andre»                	bokstav-ord
ordnr  8  (43-44): « »                    	nonword
ordnr  9  (44-53): «setningen»            	bokstav-ord
ordnr 10  (53-54): «,»                    	nonword


Setning nr 1:     (fra:0 til:50)
«Dette er  en   setning    med     lang spacing...
»
Oppdeling i ord:

ordnr  1  ( 0- 5): «Dette»                	bokstav-ord
ordnr  2  ( 5- 6): « »                    	nonword
ordnr  3  ( 6- 8): «er»                   	bokstav-ord
ordnr  4  ( 8-10): «  »                   	nonword
ordnr  5  (10-12): «en»                   	bokstav-ord
ordnr  6  (12-15): «   »                  	nonword
ordnr  7  (15-22): «setning»              	bokstav-ord
ordnr  8  (22-26): «    »                 	nonword


«Målet er 2 av 3, eller 2/3.
»
Oppdeling i ord:

ordnr  1  ( 0- 6): «Målet»               	bokstav-ord
ordnr  2  ( 6- 7): « »                    	nonword
ordnr  3  ( 7- 9): «er»                   	bokstav-ord
ordnr  4  ( 9-10): « »                    	nonword
ordnr  5  (10-11): «2»                    	tall
ordnr  6  (11-12): « »                    	nonword
ordnr  7  (12-14): «av»                   	bokstav-ord
ordnr  8  (14-15): « »                    	nonword
ordnr  9  (15-16): «3»                    	tall
ordnr 10  (16-17): «,»                    	nonword
ordnr 11  (17-18): « »                    	nonword
ordnr 12  (18-23): «eller»                	bokstav-ord
ordnr 13  (23-24): « »                    	nonword
ordnr 14  (24-25): «2»                    	tall
ordnr 15  (25-26): «/»                    	nonword
ordnr 16  (26-27): «3»                    	tall
ordnr 17  (27-28): «.»                    	nonword
ordnr 18  (28-29): «
»                    	nonwordsym:.

Siste "nonwordsym" skal ikke være med, da det er starten på neste setning.

Det er ikke mulig å ha flere bokstavord/tallord rett etter hverandre. 
Det KAN være flere nonword etter hverandre. Litt uvanlig i tekst, vanlig i kildekode o.l.
«22.44,5454,5;3.2» regnes som ett tallord.

Mulige ordklasser (for bokstav/tallord):

Separate klasser for setninger avsluttet med «.» «?» «!» «lflf»

4× ettordsetning
4× 1 og 2 i to-ordsetning
4× 1, 2 og 3 i tre-ordsetning
4× første andre tredje i setning med minst 4 ord
4× tredjesist andresist sist i setning med minst 4 ord
4× indre ord i setning med over 6 ord
første andre tredje før og etter komma
1.2.3 før og etter tallord. (regner med at etter tallord kommer ofte enheter som «liter»)
	
64 klasser så langt. Trengs alle? Bør kjøre teststatistikk på noen kjente filer
Kan se på kolon, semikolon, tekst i matchende parenteser av mange slag, anførselstegn...


Setning spesifiseres som:
punktumsetning, 11 ord, mønster OnOnO,nOnO,OnnOnOnOnnOnnnO (O=Ord, n=non-ord


enwik8 har xml, må evt. håndteres sep.
enwik8 bruker «[[» og «]]»   (wiki-lenker)
              «{{» og «}}»   (ekst. lenker)
Overskrifter med ==tekst tekst==

*/

/*
Problem:
lange_ord_med_mange_underscores
Mange av single-ordene?
Kan _godta _ord_ som_ har underscore bare på endene. 
Muligvis korte som A_1, C_4, p_q x_ij men ikke_denne.
Ellers: lage en "delsetning" av det lange ordet. Deletegn
_:.
Delsetninger:
«Battle_of_Stalingrad.htm»
«17_Flying_Fortresses.jpg»
«2C_son_of_Charles_Martel»
«Author:Fyodor_Dostoevsky»
«AutoCAD_related_articles»
«Brave_New_World_argument»
«Category:Data_structures»
«Category:Liberal_parties»
«Image:A2600_Breakout.png»
«Image:Admiralty_Flag.gif»
«Media:Albanian_shqip.ogg»
«List_of_cities_in_Canada»
«Queen_of_the_Netherlands»
«broadcasting_house.shtml»
«fr:Match_d'improvisation»
«image:Conifer_forest.jpg»
«image:Harvard_Square.JPG»
«Category:National_symbols»
«text:Iolanda_Balas_1,85_m»
«text:Iolanda_Balas_1,90_m»
«Template:British_Rail_Spot_Hire_Companies»
«Wikipedia:Disambiguation_pages_with_links»
«List_of_colleges_and_universities_starting_with_A»

Det er 12549 ord som inneholder _, og 4296 som har flere _
82446 inneholder :, ofte «fi:ord» men også «image:...»
198 inneholder to :,
57411 inneholder .   noen forkortelser, og veldig mange filnavn
Filnavn er ofte et vanlig ord, fulgt av .png, .pdf, .html o.l.
23703 inneholder flere .  noen forkortelser, veldig mange
sites som «www.et.sted.com» og tags som «image:bristol.zoo.jpg»
Forfattere som «C.S.Lewis»

punktum,underscore: Hvis <=5 bokstaver, behold det som ett ord
bokstav.bokstav.bokstav... aldri mer enn en bokstav foran punktum => forkortelse, behold som ett ord. Fanger opp «C.S.Lewis»
Alltid splitt på «:», medmindre det er på enden av ordet.
Aldri splitt på enden?


Ord som inneholder minst en av :._  141829
Ord med minst to: 34732
Kan ha egen statistikk på hva som forekommer i slike delsetninger:
De starter ofte med fi:,image:Category:,www og ender gjerne i filendelser
Andre-ord er også interessant: «List_of_...», «...co.ac.uk»
Ellers er det mye vanlige ord, og noen rariteter.
Kan nok kutte en god del i de «unike» ordene, og få kortere ordliste.
 */

//videre plan:
//1: tåle setninger med et linjeskift inni (men ikke to på rad)  OK
//2: Oppdage ulike typer setninger, basert på "." "!" "?" "\n\n" OK
//3: Oppdage ulike ord. "vanlige", "spacing"  tegnsetting? tallord? 
//4: oppdage komma, semikolon, <tags>?, sitattegn



