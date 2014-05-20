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
  cerr << "INFO: version 1.0.0 ; date: April 2014 ; author: Zev Kronenberg; email : zev.kronenberg@utah.edu " << endl;
}

void printHelp(void){
  cerr << endl << endl;
  cerr << "INFO: help" << endl;
  cerr << "INFO: description:" << endl;
  cerr << "     pFst is a probabilistic approach for detecting differences in allele frequencies between two populations,                             " << endl;
  cerr << "     a target and background.  pFst uses the conjugated form of the beta-binomial distributions to estimate                                " << endl;
  cerr << "     the posterior distribution for the background's allele frequency.  pFst calculates the probability of observing                       " << endl;
  cerr << "     the target's allele frequency given the posterior distribution of the background. By default                                          " << endl;
  cerr << "     pFst uses the genotype likelihoods to estimate alpha, beta and the allele frequency of the target group.  If you would like to assume " << endl;
  cerr << "     all genotypes are correct set the count flag equal to one.                                    " << endl << endl;

  cerr << "Output : 3 columns :     "    << endl;
  cerr << "     1. seqid            "    << endl;
  cerr << "     2. position         "    << endl;
  cerr << "     3. pFst probability "    << endl  << endl;

  cerr << "INFO: usage:  pFst --target 0,1,2,3,4,5,6,7 --background 11,12,13,16,17,19,22 --file my.vcf --deltaaf 0.1" << endl;
  cerr << endl;
  cerr << "INFO: required: t,target     -- a zero based comma seperated list of target individuals corrisponding to VCF columns"         << endl;
  cerr << "INFO: required: b,background -- a zero based comma seperated list of background individuals corrisponding to VCF columns"     << endl;
  cerr << "INFO: required: f,file       -- a properly formatted VCF.                                                               "     << endl;
  cerr << "INFO: required: y,type       -- genotype likelihood format ; genotypes: GP,GL or PL; pooled: PL                            "  << endl;
  cerr << "INFO: optional: d,deltaaf    -- skip sites where the difference in allele frequencies is less than deltaaf, default is zero"  << endl;
  cerr << "INFO: optional: c,counts     -- use genotype counts rather than genotype likelihoods to estimate parameters, default false"   << endl;
  cerr << endl;

  printVersion() ;
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

  // pooled or genotyped
  
  int pool = 0;

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
  double daf  = 0;

  // 

  int counts = 0;

  // type pooled GL PL

  string type = "NA";

    const struct option longopts[] = 
      {
	{"version"   , 0, 0, 'v'},
	{"help"      , 0, 0, 'h'},
	{"counts"    , 0, 0, 'c'},
        {"file"      , 1, 0, 'f'},
	{"target"    , 1, 0, 't'},
	{"background", 1, 0, 'b'},
	{"deltaaf"   , 1, 0, 'd'},
	{"region"    , 1, 0, 'r'},
	{"type"      , 1, 0, 'y'},
	{0,0,0,0}
      };

    int index;
    int iarg=0;

    while(iarg != -1)
      {
	iarg = getopt_long(argc, argv, "r:d:t:b:f:y:chv", longopts, &index);
	
	switch (iarg)
	  {
	  case 'h':
	    printHelp();
	    return 0;
	  case 'v':
	    printVersion();
	    return 0;
	  case 'y':	    
	    type = optarg;
	    cerr << "INFO: genotype likelihoods set to: " << type << endl;
	    break;
	  case 'c':
	    cerr << "INFO: using genotype counts rather than genotype likelihoods" << endl;
	    counts = 1;
	    break;
	  case 't':
	    loadIndices(ib, optarg);
	    cerr << "INFO: There are " << ib.size() << " individuals in the target" << endl;
	    cerr << "INFO: target ids: " << optarg << endl;
	    break;
	  case 'b':
	    loadIndices(it, optarg);
	    cerr << "INFO: There are " << it.size() << " individuals in the background" << endl;
	    cerr << "INFO: background ids: " << optarg << endl;
	    break;
	  case 'f':
	    cerr << "INFO: File: " << optarg  <<  endl;
	    filename = optarg;
	    break;
	  case 'd':
	    cerr << "INFO: only scoring sites where the allele frequency difference is greater than: " << optarg << endl;
	    deltaaf = optarg;
	    daf = atof(deltaaf.c_str());	    
	    break;
	  case 'r':
            cerr << "INFO: set seqid region to : " << optarg << endl;
	    region = optarg; 
	    break;
	  default:
	    break;
	  }
      }

    
    if(filename == "NA"){
      cerr << "FATAL: did not specify the file\n";
      printHelp();
      exit(1);
    }
    
    variantFile.open(filename);
    
    if(region != "NA"){
      variantFile.setRegion(region);
    }

    if (!variantFile.is_open()) {
      exit(1);
    }
    map<string, int> okayGenotypeLikelihoods;
    okayGenotypeLikelihoods["PL"] = 1;
    okayGenotypeLikelihoods["PO"] = 1;
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

      if(var.alt.size() > 1){
	continue;
      }
      
      map<string, map<string, vector<string> > >::iterator s     = var.samples.begin(); 
      map<string, map<string, vector<string> > >::iterator sEnd  = var.samples.end();
        
      vector < map< string, vector<string> > > target, background, total;
	        
	int index = 0;

        for (; s != sEnd; ++s) {

            map<string, vector<string> >& sample = s->second;

	    if(sample["GT"].front() != "./."){
	      if(it.find(index) != it.end() ){
		target.push_back(sample);		
		total.push_back(sample);		
	      }
	      if(ib.find(index) != ib.end()){
		background.push_back(sample);
		total.push_back(sample);		
	      }
	    }
            
	index += 1;
	}
	
	zvar * populationTarget        ;
	zvar * populationBackground    ;
	zvar * populationTotal         ;

	if(type == "PO"){
	  populationTarget     = new pooled();
          populationBackground = new pooled();
          populationTotal      = new pooled();
	}
	if(type == "PL"){
	  populationTarget     = new pl();
	  populationBackground = new pl();
	  populationTotal      = new pl();
	}
	if(type == "GL"){
	  populationTarget     = new gl();
	  populationBackground = new gl();
	  populationTotal      = new gl();
	  
	}
	if(type == "GP"){
	  populationTarget     = new gp();
	  populationBackground = new gp();
	  populationTotal      = new gp();
	}
	
	
	populationTarget->loadPop(target        , var.sequenceName, var.position);
	populationBackground->loadPop(background, var.sequenceName, var.position);
	populationTotal->loadPop(total          , var.sequenceName, var.position);

	if(populationTarget->npop < 2 || populationBackground->npop < 2){
	  continue;
	}
	
	populationTarget->estimatePosterior();
	populationBackground->estimatePosterior();
	populationTotal->estimatePosterior();

	if(populationTarget->alpha == -1 || populationBackground->alpha == -1){
          continue;
        }

	double populationTargetEstAF = bound(populationTarget->alpha / (populationTarget->alpha + populationTarget->beta));

	if(counts == 1){
	  double alpT = 0.001;
	  double betT = 0.001;
	  double alphTol = 0.001;
	  double betaTol = 0.001;
	  populationTarget->alpha = alpT + populationTarget->nref;
	  populationTarget->beta  = betT + populationTarget->nalt;
	  populationTotal->alpha  = alphTol + populationTotal->nref;
	  populationTotal->beta   = betaTol + populationTotal->nalt;
	  
	}

	double tAlt  = log(r8_beta_pdf(populationTarget->alpha, populationTarget->beta, populationTargetEstAF));
	double tNull = log(r8_beta_pdf(populationTotal->alpha, populationTotal->beta, populationTargetEstAF ));

	double l = 2* (tAlt - tNull);

	int     which = 1;
	double  p ;
	double  q ;
	double  x = l ;
	double  df = 1;
	int     status;
	double  bound ;
	

	cdfchi(&which, &p, &q, &x, &df, &status, &bound );
	
	cout << var.sequenceName << "\t"  << var.position << "\t" << 1-p << endl ;
	
	delete populationTarget;
	delete populationBackground;
	delete populationTotal;
	
	populationTarget     = NULL;
	populationBackground = NULL;
	populationTotal      = NULL;

    }
    return 0;		    
}
