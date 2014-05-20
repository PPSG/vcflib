#include "Variant.h"
#include "split.h"
#include "cdflib.hpp"
#include "pdflib.hpp"
#include "var.hpp"

#include <string>
#include <iostream>
#include <math.h>  
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <getopt.h>

using namespace std;
using namespace vcf;

void printVersion(void){
  cerr << "INFO: version 1.1.0 ; date: April 2014 ; author: Zev Kronenberg; email : zev.kronenberg@utah.edu " << endl;
}

void printHelp(void){

  cerr << endl << endl;
  cerr << "INFO: help" << endl;
  cerr << "INFO: description:" << endl;
  cerr << "      wcFst is Weir & Cockerham's Fst for two populations.  Negative values are VALID,  " << endl;
  cerr << "      they are sites which can be treated as zero Fst. For more information see Evolution, Vol. 38 N. 6 Nov 1984. " << endl;
  cerr << "      Specifically wcFst uses equations 1,2,3,4.                                                              " << endl << endl;

  cerr << "Output : 3 columns :                 "    << endl;
  cerr << "     1. seqid                        "    << endl;
  cerr << "     2. position                     "    << endl;
  cerr << "     3. target allele frequency      "    << endl;
  cerr << "     4. background allele frequency  "    << endl;
  cerr << "     5. wcFst                        "    << endl  << endl;

  cerr << "INFO: usage:  wcFst --target 0,1,2,3,4,5,6,7 --background 11,12,13,16,17,19,22 --file my.vcf --deltaaf 0.1                  " << endl;
  cerr << endl;
  cerr << "INFO: required: t,target     -- a zero based comma seperated list of target individuals corrisponding to VCF columns        " << endl;
  cerr << "INFO: required: b,background -- a zero based comma seperated list of background individuals corrisponding to VCF columns    " << endl;
  cerr << "INFO: required: f,file       -- proper formatted VCF                                                                        " << endl;
  cerr << "INFO: required, y,type       -- genotype likelihood format; genotype : GL,PL,GP                                             " << endl;
  cerr << "INFO: optional: d,deltaaf    -- skip sites where the difference in allele frequencies is less than deltaaf, default is zero "   << endl;

  printVersion();
}


double bound(double v){
  if(v <= 0.00001){
    return  0.00001;
  }
  if(v >= 0.99999){
    return 0.99999;
  }
  return v;
}

void loadIndices(map<int, int> & index, string set){
  
  vector<string>  indviduals = split(set, ",");

  vector<string>::iterator it = indviduals.begin();
  
  for(; it != indviduals.end(); it++){
    index[ atoi( (*it).c_str() ) ] = 1;
  }
}


int main(int argc, char** argv) {

  // set the random seed for MCMC

  srand((unsigned)time(NULL));

  // the filename

  string filename = "NA";

  // set region to scaffold

  string region = "NA"; 

  // using vcflib; thanks to Erik Garrison 

  VariantCallFile variantFile;

  // zero based index for the target and background indivudals 
  
  map<int, int> it, ib;
  
  // deltaaf is the difference of allele frequency we bother to look at 

  string deltaaf ;
  double daf  = 0.00;

  // genotype likelihood format

  string type = "NA";


    const struct option longopts[] = 
      {
	{"version"   , 0, 0, 'v'},
	{"help"      , 0, 0, 'h'},
        {"file"      , 1, 0, 'f'},
	{"target"    , 1, 0, 't'},
	{"background", 1, 0, 'b'},
	{"deltaaf"   , 1, 0, 'd'},
	{"type"      , 1, 0, 'y'},
	{"region"    , 1, 0, 'r'},
	{0,0,0,0}
      };

    int index;
    int iarg=0;

    while(iarg != -1)
      {
	iarg = getopt_long(argc, argv, "y:r:d:t:b:f:chv", longopts, &index);
	
	switch (iarg)
	  {
	  case 'h':
	    printHelp();
	    return 0;
	  case 'v':
	    printVersion();
	    return 0;
	  case 't':
	    loadIndices(it, optarg);
	    cerr << "INFO: there are " << it.size() << " individuals in the target" << endl;
	    cerr << "INFO: target ids: " << optarg << endl;
	    break;
	  case 'b':
	    loadIndices(ib, optarg);
	    cerr << "INFO: there are " << ib.size() << " individuals in the background" << endl;
	    cerr << "INFO: background ids: " << optarg << endl;
	    break;
	  case 'f':
	    cerr << "INFO: file: " << optarg  <<  endl;
	    filename = optarg;
	    break;
	  case 'd':
	    cerr << "INFO: only scoring sites where the allele frequency difference is greater than: " << optarg << endl;
	    deltaaf = optarg;
	    daf = atof(deltaaf.c_str());	    
	    break;
	  case 'y':
	    type = optarg;
	    cerr << "INFO: setting genotype likelihood format to: " << type << endl;
	    break;
	  case 'r':
            cerr << "INFO: set seqid region to : " << optarg << endl;
	    region = optarg; 
	    break;
	  default:
	    break;
	  }

      }
    
    variantFile.open(filename);
    
    if(region != "NA"){
      variantFile.setRegion(region);
    }

    if (!variantFile.is_open()) {
      cerr << "FATAL: could not open VCF for reading" << endl;
      printHelp();
      return 1;
    }

    map<string, int> okayGenotypeLikelihoods;
    okayGenotypeLikelihoods["PL"] = 1;
    okayGenotypeLikelihoods["GL"] = 1;
    okayGenotypeLikelihoods["GP"] = 1;

    if(type == "NA"){
      cerr << "FATAL: failed to specify genotype likelihood format : PL or GL" << endl;
      printHelp();
      return 1;
    }
    if(okayGenotypeLikelihoods.find(type) == okayGenotypeLikelihoods.end()){
      cerr << "FATAL: genotype likelihood is incorrectly formatted, only use: PL or GL" << endl;
      printHelp();
      return 1;
    }

    Variant var(variantFile);

    while (variantFile.getNextVariant(var)) {
        map<string, map<string, vector<string> > >::iterator s     = var.samples.begin(); 
        map<string, map<string, vector<string> > >::iterator sEnd  = var.samples.end();
        
	// biallelic sites naturally 

	if(var.alt.size() > 1){
	  continue;
	}
	
	vector < map< string, vector<string> > > target, background, total;
	        
	int index = 0;

        for (; s != sEnd; ++s) {

            map<string, vector<string> >& sample = s->second;

	    if(sample["GT"].front() != "./."){
	      if(it.find(index) != it.end() ){
		target.push_back(sample);
	      }
	      if(ib.find(index) != ib.end()){
		background.push_back(sample);
	      }
	    }            
	    index += 1;
	}
	
	genotype * populationTarget      ;
	genotype * populationBackground  ;

	if(type == "PL"){
	  populationTarget         = new pl();
	  populationBackground     = new pl();
	}
	if(type == "GL"){
	  populationTarget     = new gl();
	  populationBackground = new gl();
	}
	if(type == "GP"){
	  populationTarget     = new gp();
	  populationBackground = new gp();
	}
	
	populationTarget->loadPop(target, var.sequenceName, var.position);
	populationBackground->loadPop(background, var.sequenceName, var.position);

	double afdiff = abs(populationTarget->af - populationBackground->af);

        if(afdiff < daf){
          continue;
        }
	
	// pg 1360 B.S Weir and C.C. Cockerham 1984
	double nbar = ( populationTarget->ngeno / 2 ) + (populationBackground->ngeno / 2);
	double rn   = 2*nbar;
	
	// special case of only two populations
	double nc   =  rn ;
	nc -= (pow(populationTarget->ngeno,2)/rn);
	nc -= (pow(populationBackground->ngeno,2)/rn);
	// average sample frequency
	double pbar = (populationTarget->af + populationBackground->af) / 2;

	// sample variance of allele A frequences over the population 
	
	double s2 = 0;
	s2 += ((populationTarget->ngeno * pow(populationTarget->af - pbar, 2))/nbar);
	s2 += ((populationBackground->ngeno * pow(populationBackground->af - pbar, 2))/nbar);
	
	// average heterozygosity 
	
	double hbar = (populationTarget->hfrq + populationBackground->hfrq) / 2;
	
	//global af var
	double pvar = pbar * (1 - pbar);

	// a, b, c

	double avar1 = nbar / nc;
	double avar2 = 1 / (nbar -1) ;
	double avar3 = pvar - (0.5*s2) - (0.25*hbar);
	double avar  = avar1 * (s2 - (avar2 * avar3));
	
	double bvar1 = nbar / (nbar - 1);
	double bvar2 = pvar - (0.5*s2) - (((2*nbar -1)/(4*nbar))*hbar);
	double bvar  = bvar1 * bvar2;

	double cvar = 0.5*hbar;
	
	double fst = avar / (avar+bvar+cvar);
	
	cout << var.sequenceName << "\t"  << var.position << "\t" << populationTarget->af << "\t" << populationBackground->af << "\t" << fst << endl ;

    }
    return 0;		    
}
