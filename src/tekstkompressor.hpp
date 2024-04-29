/*
	tekstkompressor.hpp
	Felles strukturer for:
	icu-pakk: program som komprimerer tekst inndelt i ord
	          implementerer ::pakk();
	utpakk:   program som pakker ut teksten igjen
	          implementerer ::pakkut();
*/

#ifndef _tekstkompressor
#define _tekstkompressor

#include "aritkode.hpp"

//Minste serie med like lange endelser
//11 funnet ved eksperimentering, best for enwik8 og skriv1
//24/7 optimalt for skriv3()
#define MINSTESERIE  10
#define MINSTESERIE2 7 
//Korteste prefix/suffix
//2 eller mer
// 5 er optimalt for skriv3()
#define MINFIX 2

//SKRIV1 SKRIV3 SKRIV4 SKRIV5
#define SKRIV6

/* (Re-tuning e. oppdeling. Kun bokstavord)
              ordbok+   suffix2   som 3+    som 3/4+ ordbok
	            suffix1   ordbok    ordleng.  shortlex suffix3
Parameter     skriv1()  skriv3()  skriv4()  skriv5() skriv6()
MINSTESERIE         10        14        23        14       10
MINSTESERIE2       ---         7         4         3      ---
MINFIX             ---         7         5         3        2
resultat       1074893   1078323   1148433   1199813   990530
(bugfix)       1071706
enwik8, alle ordlister:                               1047303
enwik9: alle ordlister:                               4824101

Tatt bort:
skriv1(), fordi skriv6() gjør jobben nå. 
skriv2(), et blindspor
*/

//Max antall nuller på rad, når vi skriver ordfrekvenser
//Sannsynligvis lite å hente på tuning her.
#define NULLSERIE 10





class tekstkompressor;
class ordtre;
class ord;

/*
Ord, hvor bokstavinnholdet ikke lagres fordi det ligger fra «fra» til «til»
*/
class ord {
	public: //Gidder ikke kapsle inn en så enkel struktur!
	int fra; //Fra og med
	int til; //Til og med
	int antall; 

// -----------------------
	// HS
	int hyppig;
	// -------------------------

	//Håndtering av felles prefix
	union {
		unsigned short overlapp; //# bytes like med forrige ord i alf. liste
		unsigned short pref_stat_ix;
	};
	unsigned short endelse; //# tegn endelse, ikke del av prefix eller suffix (egentlig midttegn)
//	unsigned short startdel; //# tegn start, ikke del av suffix !!!eksperiment
	unsigned char bokstavforskjell; //Numerisk forskjell i første ulike tegn
	union {
		unsigned char fix; //exp1: 0 vanlig ord, 1:brukt som prefix, 2:suffix
											 //exp2: 0/1: vanl/suffix. 
	    	               //0/2: m/u. bforskjell 
											 //0/4: u/m garantert suffix
											 //0/8 brukes av skriv6
		unsigned char typ;
		//Etter ordbokfasen:
		//0: nonord
		//1: ord
		//2: tallord
		//3: annet, setningsinfo o.l.
		//Håndtering av felles endelser
	};
	union {
		unsigned short overlapp2; //Overlapp med slutt på neste ord
		unsigned short suff_stat_ix;
	};
	int endelse2;//index til endelsesord
	//Håndtering av prefix-experiment!!!
	union {
		int start2; //index til prefix (eksperiment 1)
		int nr; //nr i sortert rekkefølge
	};
	int lengde();
	void ny(int f, int t, int ant);
	void entil(int ant);
	// HS --------------------------------------
	void sett_hyppig(int index);  // HS Setter hyppig lik plassen ordet får i en sortert liste på antall forekomster
	// --------------------------------------
	friend class ordtre;
	friend class trenode;
};


class ordlager_c {
	int antall;
	int kapasitet;
	ord *ordtabell;
friend class tekstkompressor;
	public: 
	ordlager_c(int kapasitet); 
	int nyord(int fra, int til, int ant);
	ord *hent(int ordnr);
	int les_antall();	
};

//stats for forsking, finne fordelingen av en mengde ints
class statistikk {
	int *t;
	int min, max;   //min og max som kan registreres
	int tmin, tmax; //min og max i datasettet
	char const *navn;

	public:
	statistikk(char const *pnavn, int pmin, int pmax);
	~statistikk();
	void reg(int dat);
	inline void rapport();
};


class ordstakk {
	ord **tab;
	int kapasitet;
	public:
	int sp;
	ordstakk(int kap);
	~ordstakk();
	void push(ord *o);
	ord *pop();
};

//For div. statistikker om ordlister,for kompresjon
class listestat {
	unsigned int min_i; //Laveste entry >0, norm. 0 eller 1
	unsigned int max_i; //høyeste index
	unsigned int max_f;  //høyeste frekvens
	unsigned int max_f2; //nest høyeste frekvens
	unsigned int *tab; //tabell opp til tab[max]
	unsigned int *akk;	//Akkumulerte frekvenser
	unsigned int fsum; //Sum av alle frekvenser
	unsigned int *ix; //permutasjonsliste for nivå1-stats
	int ant_ix; //antall frekvenser som ikke er 0, etter ixsort()
	bool akkumulert;
	public:
	listestat(unsigned int pmin, unsigned int pmax);
 	~listestat();
	void tell(unsigned int tall);
	void add(unsigned int ix, unsigned int antall);
	void akkumuler();
	void finnmax();
	void til_fil(tekstkompressor *tk, int maxgrense_i, int sumf_grense);
	void til_fil_ix(tekstkompressor *tk, int sumf_grense, listestat *ls_ix);
	void kod(unsigned int x, tekstkompressor *tk);
	void kod_nix0(unsigned int x, tekstkompressor *tk);
	void kod_grense(unsigned int x, unsigned int max, tekstkompressor *tk);
	void dump(); //debug/analyse
	void ixsort(); //Lag rekkefølge for nivå1

	friend class tekstkompressor;
	friend class ordtre;  // HS! pass på!
};


/*
Binært søketre, for:
 * å finne ut om et ord er nytt, eller sett før
 * å få ut ordlista i sortert orden
   Muligvis nyttig for å lage komprimert blob?
Ordene legges i ordlageret.
*/
class trenode {
	trenode *v, *h;
	int ordnr; //Index tar mindre plass enn enn peker (64-bit)
	int listenr; //nr i den sorterte rekkefølgen
	friend class ordtre;
	friend class tekstkompressor;

	public:
	trenode (int onr); 
	void skriv_inorden(statistikk **stats, ordlager_c *ordlager);
	void lagstats(int fase, ordlager_c *ordlager);
	void kompr_ordliste(tekstkompressor *tk, int, int, ordlager_c *);
	void tre_til_liste(int *liste, int *nr);
};

typedef int (ordtre::*functype)(int fra, int til, ord *o); 

class ordtre {
	private:
	trenode *rot;
	functype sammenlign;
	tekstkompressor *tkmp;

  //int antall;     //Antall unike ord i treet. (Duplikater blir ikke nye ord) erst. av ordlager->antall
	int total;			//Antall, inkludert duplikater
	int maxrep;			//Antal reps for det mest repeterte ordet
	int single;			//Antall ord som bare står 1 gang
	int maxlengde; 	//Lengden på det lengste ordet

	int sammenlign1(int fra, int const til, ord *o);
	int sammenlign_bakl(int const fra, int til, ord *o); //baklengs
	int sammenlign_shortlex(int fra, int til, ord *o);

	public:
	ordlager_c *ordlager; //Ordlageret til dette treet (og baklengstreet)
	char const *navn;     //const, så ingen grunn til å ha aksessfunk!
	ordtre(char const * const nytt_navn, ordlager_c *olager, tekstkompressor *tkmp);
	ordtre(char const * const nytt_navn, ordlager_c *olager, functype sammenlignf, tekstkompressor *tkmp);
	inline int lesantall() { return ordlager->les_antall(); }
	inline int lestotal() { return total; }
	inline int lesmaxrep() { return maxrep; }
	inline int lessingle() { return single; }
	inline float snittreps() { return (float)total/lesantall(); }
	inline int lesmaxlengde() { return maxlengde; }

	listestat **folger_stat;  // HS
	listestat *ordfordeling; // HS

	void nyord2(int ordnr);
	void nyord3(int ordnr);
	int nyord(int fra, int til, ordtre *baklengstre, int ant);
	int oppslag(int fra, int til);
	ord *tegnoppslag(char c);
	void skrivordliste();
	void merk(int merke, int offset);
	friend class tekstkompressor;
};

class tekstkompressor : public aritkode {
	private:
	int verbose; //0-9
	int ant_unike_ord;
	int max_ordlengde;
	FILE *inn, *ut;
	char const *innfilnavn;
	char const *utfilnavn;
	ordtre *bokstavordtre;
	ordtre *baklengstre;
	ordtre *tallordtre;
	ordtre *bakltall;
	ordtre *nonordtre;

	ord **tekst; //Hele teksten, ordene i rekkefølge
	int ant_ord;

	// HS:  --------------
	int *setninger;   // 0, 1.setn->til, 2.setn->til, ... (lengde lik ant_setn +1) (til, ikke tom.)
	int ant_setn;  //HS Kan kanskje fjernes
	// HS ------------
	int cutoff;


	void skrivtxtstat1(listestat *txt, listestat txt1[256]);

	//Metoder som skriver komprimerte ordlister
	void skriv3(ordtre *tre, ordtre *btre);
	void skriv4(ordtre *tre, ordtre *btre);
	void skriv5(ordtre *tre, ordtre *btre);
	void skriv6(ordtre *tre, ordtre *btre);

	void reg_bokstavord1(int fra, int til);
	void reg_bokstavord2(int fra, int til);
	void reg_bokstavord3(int fra, int til);
	void reg_bokstavord4(int fra, int til);
	void reg_bokstavord5(int fra, int til);

	void reg_tallord1(int fra, int til);
	void reg_tallord2(int fra, int til);

	void dump_tekst();

	//Metoder som skriver komprimert tekst
	void kod_tekst1();
	void kod_tekst2();
	void kod_tekst3();

	void antallstats(int *liste, int ant, ordtre *tre);

	public:
	tekstkompressor(char const *innf, char const *utf, bool pakkinn, int verb, int cutoff);
	void reg_tekst(ord *o);
	void pakk();
	void pakkut();

// HS------------------------------------------------------------------
	void reg_setn(int end);   // Legger til setnings->til nummer i 'setninger'
	void hs_stat2();
	void beregn_entropi(listestat *forste_ord_ls, listestat *andre_ord_ls, listestat *tredje_ord_ls, listestat *nest_nest_siste_ls, 
	listestat *nest_siste_ls, listestat *siste_ls, listestat *pre_komma_ls, listestat *post_komma_ls,  listestat **folger_ls, int ant_folger_ls, int *setningslengder);

	// HS -----------------------------------------------------
};

#endif
