/*
	tekstkompressor.cpp
	felles metoder som brukes både av inn- og utpakking
	 */

#include <stdio.h>
#include "tekstkompressor.hpp"

tekstkompressor::tekstkompressor(char const *innf, char const *utf, bool pakkinn, int verb) : aritkode(pakkinn ? ut=fopen(utf,"w") : inn=fopen(innf, "r")) 
{
	innfilnavn = innf;
	utfilnavn = utf;
	verbose = verb;
	if (pakkinn)	inn = fopen(innfilnavn, "r");
	else ut = fopen(utfilnavn, "w"); 
	if (!inn) throw("i/o innfil");
	if (!ut) throw("i/o utfil");
	tekst = new ord*[500000000]; //50000000 Nok for enwik8. Mer for enwik9
	ant_ord = 0;
	//HS:
	setninger = new int[10000000];  // 5 ord per setning om antall ord er lik grensen satt over. 
	ant_setn = 0;
}

listestat::listestat(unsigned int pmin, unsigned int pmax) {
	min_i = pmin;
	max_i = pmax;
	tab = new unsigned int[max_i+1]();
	akk = new unsigned int[max_i+1]();
	max_f = max_f2 = fsum = 0;
	ix = NULL;
	akkumulert = false;
}

listestat::~listestat() {
	delete tab;
	delete akk;
	if (ix) delete ix;
}

void listestat::ixsort() {
	unsigned int tmp[max_i+1];
	ix = new unsigned int[max_i+1];
	//Kopier frekvenser, og initialiser ix
	for (int i=0; i <= max_i; ++i) {
		tmp[i] = tab[i];
		ix[i] = i;
	}
	//sorter tmp, og endre ix tilsvarende
	for (int j = 1 ; j <= max_i; ++j) {
		int midl = tmp[j], midl_ix = ix[j];
		int i = j - 1;
		for (; i >= 0 && tmp[i] < midl; --i) {
			tmp[i+1] = tmp[i];
			ix[i+1] = ix[i];
		}
		++i;
		tmp[i] = midl;
		ix[i] = midl_ix;
	}
	int i;
	for (i = max_i; i > 0 && tmp[i] == 0; --i) ;
	ant_ix = i+1; //Antall frekvenser som IKKE er 0
}
