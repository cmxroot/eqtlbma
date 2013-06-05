/** \file eqtlbma_avg_bfs.cpp
 *
 *  `eqtlbma_avg_bfs' averages the raw BFs, over the grid only, or over both
 *  the grid and the configurations.
 *  Copyright (C) 2013 Timothee Flutre
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  g++ -Wall -Wextra -g eqtlbma_avg_bfs.cpp eqtlbma/utils/utils_io.cpp eqtlbma/utils/utils_math.cpp -I./eqtlbma -lgsl -lgslcblas -lz -o eqtlbma_avg_bfs
 */

#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <cmath>
#include <libgen.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <limits>
#include <sstream>
#include <numeric>
using namespace std;

#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_cdf.h>

#include "utils/utils_io.hpp"
#include "utils/utils_math.hpp"
using namespace utils;

#ifndef VERSION
#define VERSION "0.0"
#endif

/** \brief Display the help on stdout.
 */
void help(char ** argv)
{
  cout << "`" << argv[0] << "'"
       << " averages the raw BFs over the grid only, or over both the grid and the configurations." << endl
       << endl
       << "Usage: " << argv[0] << " [OPTIONS] ..." << endl
       << endl
       << "Options:" << endl
       << "  -h, --help\tdisplay the help and exit" << endl
       << "  -V, --version\toutput version information and exit" << endl
       << "  -v, --verbose\tverbosity level (0/default=1/2/3)" << endl
       << "      --in\tpatterns to glob '_l10abfs_raw' files from 'eqtlbma_bf'" << endl
       << "      --gwts\tfile with grid weights (one per line, only the value)" << endl
       << "      --cwts\tfile with configuration weights (one per line, name<sep>value)" << endl
       << "      --save\tprecise what to save (bf/post/bf+post)" << endl
       << "\t\t'post' requires also options --pi0 and --post" << endl
       << "      --pi0\tif not provided, BFs will be saved instead of posterior probability" << endl
       << "      --post\tsave various kinds of posterior probabilities (e.g. 'a+b')" << endl
       << "\t\ta: the gene has at least one eQTL" << endl
       << "\t\tb: the SNP is 'the' eQTL for the gene, in at least one subgroup, given that the gene has exactly one eQTL,\n\t\tassuming all cis SNPs are equally likely and a single eQTL per gene" << endl
       << "\t\tc: the SNP is 'an' eQTL for the gene, in at least one subgroup, given that the gene contains at least one eQTL\n\t\tand that SNPs are independent" << endl
       << "\t\td: the SNP is an eQTL in subgroup s, given that it is the eQTL for the gene" << endl
       << "      --config\talso save one BF and/or posterior for each configuration" << endl
       << "      --out\tname of the output file (gzipped)" << endl
       << "\t\tif --cwts is not provided, the output file will be used as input for 'eqtlbma_hm'" << endl
       << endl;
}

/** \brief Display version and license information on stdout.
 */
void version(char ** argv)
{
  cout << argv[0] << " " << VERSION << endl
       << endl
       << "Copyright (C) 2013 Timothee Flutre." << endl
       << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>" << endl
       << "This is free software; see the source for copying conditions.  There is NO" << endl
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << endl
       << endl
       << "Written by Timothee Flutre." << endl;
}

/** \brief Parse the command-line arguments and check the values of the 
 *  compulsory ones.
 */
void
parseCmdLine(
  int argc,
  char ** argv,
  int & verbose,
  string & file_pattern,
  string & file_grid_weights,
  string & file_config_weights,
  vector<string> & quantities_to_save,
  double & pi0,
  vector<string> & post_probas_to_save,
  bool & save_configs,
  string & file_hm)
{
  int c = 0;
  while(true){
    static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'V'},
      {"verbose", required_argument, 0, 'v'},
      {"in", required_argument, 0, 0},
      {"gwts", required_argument, 0, 0},
      {"cwts", required_argument, 0, 0},
      {"save", required_argument, 0, 0},
      {"pi0", required_argument, 0, 0},
      {"post", required_argument, 0, 0},
      {"config", no_argument, 0, 0},
      {"out", required_argument, 0, 0},
      {0, 0, 0, 0}
    };
    int option_index = 0;
    c = getopt_long(argc, argv, "hVv:",
		    long_options, &option_index);
    if(c == -1)
      break;
    switch (c){
    case 0:
      if(long_options[option_index].flag != 0)
	break;
      if(strcmp(long_options[option_index].name, "in") == 0){
	file_pattern = optarg;
	break;
      }
      if(strcmp(long_options[option_index].name, "gwts") == 0){
	file_grid_weights = optarg;
	break;
      }
      if(strcmp(long_options[option_index].name, "cwts") == 0){
	file_config_weights = optarg;
	break;
      }
      if(strcmp(long_options[option_index].name, "save") == 0){
	split(optarg, "+", quantities_to_save);
	break;
      }
      if(strcmp(long_options[option_index].name, "pi0") == 0){
	pi0 = atof(optarg);
	break;
      }
      if(strcmp(long_options[option_index].name, "out") == 0){
	file_hm = optarg;
	break;
      }
      if(strcmp(long_options[option_index].name, "post") == 0){
	split(optarg, "+", post_probas_to_save);
	break;
      }
      if(strcmp(long_options[option_index].name, "config") == 0){
	save_configs = true;
	break;
      }
    case 'h':
      help(argv);
      exit(0);
    case 'V':
      version(argv);
      exit(0);
    case 'v':
      verbose = atoi(optarg);
      break;
    case '?':
      printf("\n"); help(argv);
      abort();
    default:
      printf("\n"); help(argv);
      abort();
    }
  }
  if(file_pattern.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --bf-out" << endl << endl;
    help(argv);
    exit(1);
  }
  if(file_grid_weights.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --gwts" << endl << endl;
    help(argv);
    exit(1);
  }
  if(! doesFileExist(file_grid_weights)){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: can't find file " << file_grid_weights << endl << endl;
    help(argv);
    exit(1);
  }
  if(! file_config_weights.empty() && ! doesFileExist(file_config_weights)){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: can't find file " << file_config_weights << endl << endl;
    help(argv);
    exit(1);
  }
  if(file_hm.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --hm-in" << endl << endl;
    help(argv);
    exit(1);
  }
  if(find(quantities_to_save.begin(), quantities_to_save.end(), "post")
     != quantities_to_save.end() && post_probas_to_save.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --post with --save post" << endl << endl;
    help(argv);
    exit(1);
  }
  if(! isNan(pi0) && post_probas_to_save.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --post with --pi0" << endl << endl;
    help(argv);
    exit(1);
  }
  if(isNan(pi0) && ! post_probas_to_save.empty()){
    cerr << "cmd-line: " << getCmdLine(argc, argv) << endl << endl
	 << "ERROR: missing compulsory option --pi0 with --post" << endl << endl;
    help(argv);
    exit(1);
  }
}

class BayesFactor
{
public:
  string gene;
  string snp;
  string config;
  double bf;
  BayesFactor(void);
  BayesFactor(const string & g, const string & s, const string & c);
  void avg_raw_bfs(const vector<string> & tokens,
		   const vector<double> & grid_weights);
};

BayesFactor::BayesFactor(void)
{
}

BayesFactor::BayesFactor(const string & g, const string & s, const string & c)
{
  gene = g;
  snp = s;
  config = c;
}

void BayesFactor::avg_raw_bfs(const vector<string> & tokens,
			      const vector<double> & grid_weights)
{
  vector<double> values(tokens.size(), NaN);
  for(size_t i = 0; i < tokens.size(); ++i)
    values[i] = atof(tokens[i].c_str());
  bf = log10_weighted_sum(&values[0], &grid_weights[0], values.size());
}

class Snp
{
public:
  string name;
  vector<vector<double> > config_grid_log10_bfs;
  vector<double> config_log10_bfs;
  double log10_bf;
  double post_the_eQTL;
  double post_an_eQTL;
  vector<double> post_configs;
  vector<double> post_subgroups;
  size_t best_config_idx;
  Snp(void);
  Snp(const string & n, const vector<vector<double> > & vv);
  void avg_raw_bfs(const vector<double> & grid_weights,
		   const vector<double> & config_weights);
};

Snp::Snp(void)
{
}

Snp::Snp(const string & n, const vector<vector<double> > & vv)
{
  name = n;
  config_grid_log10_bfs = vv;
  log10_bf = NaN;
  best_config_idx = string::npos;
}

void Snp::avg_raw_bfs(const vector<double> & grid_weights,
		      const vector<double> & config_weights)
{
  config_log10_bfs = vector<double>(config_weights.size(), NaN);
  for(size_t i = 0; i < config_weights.size(); ++i){
    config_log10_bfs[i] = log10_weighted_sum(&config_grid_log10_bfs[i][0],
					     &grid_weights[0],
					     config_grid_log10_bfs[i].size());
    if(best_config_idx == string::npos
       || config_log10_bfs[i] > config_log10_bfs[best_config_idx])
      best_config_idx = i;
  }
  log10_bf = log10_weighted_sum(&config_log10_bfs[0],
				&config_weights[0],
				config_log10_bfs.size());
}

class Gene
{
public:
  string name;
  vector<Snp> snps;
  double log10_bf;
  double post;
  Gene(void);
  Gene(const string & n);
  void avg_raw_bfs(const vector<double> & grid_weights,
		   const vector<double> & config_weights);
  void calc_posterior(const double & pi0);
  void calc_snp_posteriors_the_eQTL(void);
  void calc_snp_posteriors_an_eQTL(void);
  void calc_snp_posteriors_config(const vector<double> & config_weights);
  void calc_snp_posteriors_subgroup(
    const vector<string> & config_names,
    const vector<double> & config_weights,
    const vector<vector<string> > & config2subgroups);
};

Gene::Gene(void)
{
}

Gene::Gene(const string & n)
{
  name = n;
  log10_bf = NaN;
  post = NaN;
}

void Gene::avg_raw_bfs(const vector<double> & grid_weights,
		       const vector<double> & config_weights)
{
  vector<double> tmp(snps.size(), NaN);
  for(size_t i = 0; i < snps.size(); ++i){
    snps[i].avg_raw_bfs(grid_weights, config_weights);
    tmp[i] = snps[i].log10_bf;
  }
  log10_bf = log10_weighted_sum(&tmp[0], tmp.size());
}

void Gene::calc_posterior(const double & pi0)
{
  if(! isNan(pi0))
    post = ((1-pi0) * pow(10,log10_bf))
      / (pi0 + (1-pi0) * pow(10,log10_bf));
}

void Gene::calc_snp_posteriors_the_eQTL(void)
{
  double sum_BF_kp = 0.0;
  for(size_t i = 0; i < snps.size(); ++i)
    sum_BF_kp += pow(10, snps[i].log10_bf);
  
  for(size_t i = 0; i < snps.size(); ++i)
    snps[i].post_the_eQTL = pow(10, snps[i].log10_bf) / sum_BF_kp;
}

void Gene::calc_snp_posteriors_an_eQTL(void)
{
  for(size_t i = 0; i < snps.size(); ++i)
    snps[i].post_an_eQTL = ((1/(double)snps.size()) * pow(10, snps[i].log10_bf))
      / ((1/(double)snps.size()) * pow(10, snps[i].log10_bf) + (1 - 1/(double)snps.size()));
}

void Gene::calc_snp_posteriors_config(const vector<double> & config_weights)
{
  for(size_t v = 0; v < snps.size(); ++v){
    snps[v].post_configs = vector<double>(config_weights.size(), NaN);
    for(size_t j = 0; j < config_weights.size(); ++j){
      snps[v].post_configs[j] = (config_weights[j] * pow(10, snps[v].config_log10_bfs[j]))
	/ pow(10, snps[v].log10_bf);
    }
  }
}

void Gene::calc_snp_posteriors_subgroup(
  const vector<string> & config_names,
  const vector<double> & config_weights,
  const vector<vector<string> > & config2subgroups)
{
  size_t nb_subgroups = (size_t) log2(config_names.size() + 1);
  
  stringstream subgroup_id;
  for(size_t v = 0; v < snps.size(); ++v){
    snps[v].post_subgroups = vector<double>(nb_subgroups, NaN);
    for(size_t s = 0; s < nb_subgroups; ++s){
      subgroup_id.str("");
      subgroup_id << s+1;
      snps[v].post_subgroups[s] = 0.0;
      for(size_t j = 0; j < config_names.size(); ++j){
      	if(find(config2subgroups[j].begin(), config2subgroups[j].end(), subgroup_id.str())
      	   != config2subgroups[j].end()){
      	  snps[v].post_subgroups[s] += snps[v].post_configs[j];
      	    // (config_weights[j] * pow(10, snps[v].config_log10_bfs[j]))
      	    // / pow(10, snps[v].log10_bf);
      	}
      }
    }
  }
}

void loadFileGridWeights(const string & file_grid_weights,
			 const int & verbose,
			 vector<double> & grid_weights)
{
  if(verbose > 0)
    cout <<"load grid weights ..." << endl << flush;
  
  string line;
  gzFile stream;
  vector<string> tokens;
  size_t nb_lines = 0;
  
  openFile(file_grid_weights, stream, "rb");
  
  while(getline(stream, line)){
    nb_lines++;
    split(line, " \t", tokens);
    grid_weights.push_back(atof(tokens[0].c_str()));
  }
  if(! gzeof(stream)){
    cerr << "ERROR: can't read successfully file "
	 << file_grid_weights << " up to the end" << endl;
    exit(1);
  }
  
  closeFile(file_grid_weights, stream);
  
  if(verbose > 0)
    for(size_t i = 0; i < grid_weights.size(); ++i)
      cout << "grid weight " << i+1 << ": "
	   << setprecision(4) << scientific
	   << grid_weights[i] << endl;
}

void loadFileConfigWeights(const string & file_config_weights,
			   const int & verbose,
			   vector<string> & config_names,
			   vector<double> & config_weights,
			   vector<vector<string> > & config2subgroups)
{
  if(! file_config_weights.empty()){
    if(verbose > 0)
      cout <<"load config weights ..." << endl << flush;
    
    string line;
    gzFile stream;
    vector<string> tokens;
    size_t nb_lines = 0;
    
    openFile(file_config_weights, stream, "rb");
    
    while(getline(stream, line)){
      nb_lines++;
      split(line, " \t,", tokens);
      config_names.push_back(tokens[0]);
      config_weights.push_back(atof(tokens[1].c_str()));
      config2subgroups.push_back(vector<string>());
      split(tokens[0], '-', config2subgroups[config2subgroups.size()-1]);
    }
    if(! gzeof(stream)){
      cerr << "ERROR: can't read successfully file "
	   << file_config_weights << " up to the end" << endl;
      exit(1);
    }
    
    closeFile(file_config_weights, stream);
    
    if(verbose > 0)
      for(size_t i = 0; i < config_weights.size(); ++i)
	cout << "config weight " << i+1 << " (" << config_names[i] << "): "
	     << setprecision(4) << scientific
	     << config_weights[i] << endl;
  }
}

void averageBFs(const string & file_bf_out,
		const vector<string> & lines,
		const vector<double> & grid_weights,
		vector<BayesFactor> & bfs)
{
  vector<string> tokens;
  
  split(lines[0], " \t,", tokens);
  if(tokens[0].compare("gene") != 0 && tokens[1].compare("snp") != 0
     && tokens[2].compare("config") != 0){
    cerr << "ERROR: wrong header line in file " << file_bf_out << endl;
    exit(1);
  }
  
  for(size_t i = 1; i < lines.size(); ++i){
    split(lines[i], " \t,", tokens);
    BayesFactor bf(tokens[0], tokens[1], tokens[2]);
    bf.avg_raw_bfs(vector<string> (tokens.begin()+3,
				   tokens.begin()+3+grid_weights.size()),
		   grid_weights);
    bfs.push_back(bf);
  }
}

void saveAvgBFs(const vector<BayesFactor> & bfs,
		const string & file_hm,
		gzFile & stream_hm,
		stringstream & txt,
		const size_t & nb_lines_hm)
{
  for(size_t i = 0; i < bfs.size(); ++i){
    txt.str("");
    txt << bfs[i].gene << "\t" << bfs[i].snp
	<< "\t" << bfs[i].config << "\t" << bfs[i].bf << "\n";
    gzwriteLine(stream_hm, txt.str(), file_hm, nb_lines_hm);
  }
}

void averageRawBFsOverGrid(const vector<string> & files_bf_out,
			   const vector<double> & grid_weights,
			   const string & file_hm,
			   const int & verbose)
{
  if(verbose > 0)
    cout << "average raw BFs over grid ..." << endl;
  
  gzFile stream_hm;
  openFile(file_hm, stream_hm, "wb");
  stringstream txt;
  size_t nb_lines_hm = 0;
  
  txt << "gene\tsnp\tconfig\tl10abf.grid.avg" << endl;
  nb_lines_hm = 1;
  gzwriteLine(stream_hm, txt.str(), file_hm, nb_lines_hm);
  
  vector<string> lines;
  vector<BayesFactor> bfs;
  for(size_t f = 0; f < files_bf_out.size(); ++f){
    if(verbose > 0)
      progressBar("", f+1, files_bf_out.size());
    readFile(files_bf_out[f], lines);
    averageBFs(files_bf_out[f], lines, grid_weights, bfs);
    saveAvgBFs(bfs, file_hm, stream_hm, txt, nb_lines_hm);
    lines.clear();
    bfs.clear();
  }
  if(verbose > 0)
    cerr << endl << flush;
  
  closeFile(file_hm, stream_hm);
}


void averageBFs(const string & file_bf_out,
		const vector<string> & lines,
		const vector<double> & grid_weights,
		const vector<string> & config_names,
		const vector<double> & config_weights,
		const vector<vector<string> > & config2subgroups,
		const vector<string> & quantities_to_save,
		const double & pi0,
		const vector<string> & post_probas_to_save,
		const bool & save_configs,
		vector<Gene> & genes)
{
  vector<string> tokens;
  
  split(lines[0], " \t,", tokens);
  if(tokens[0].compare("gene") != 0 && tokens[1].compare("snp") != 0
     && tokens[2].compare("config") != 0){
    cerr << "ERROR: wrong header line in file " << file_bf_out << endl;
    exit(1);
  }
  
  string curr_gene, curr_snp;
  Gene gene;
  vector<vector<double> > log10_bfs; // dim1 is config, dim2 is grid
  
  for(size_t i = 1; i < lines.size(); ++i){
    split(lines[i], " \t,", tokens);
    
    if(tokens[2].find("gen") != string::npos)
      continue;
    
    if(tokens[0].compare(curr_gene) != 0){
      if(! gene.name.empty()){
    	gene.snps.push_back(Snp(curr_snp, log10_bfs));
    	genes.push_back(gene);
      }
      curr_gene = tokens[0];
      gene = Gene(curr_gene);
      log10_bfs.clear();
    }
    
    if(tokens[1].compare(curr_snp) != 0){
      if(! log10_bfs.empty())
    	gene.snps.push_back(Snp(curr_snp, log10_bfs));
      curr_snp = tokens[1];
      log10_bfs.clear();
    }
    
    size_t config_idx = log10_bfs.size();
    log10_bfs.push_back(vector<double> (grid_weights.size(), NaN));
    for(size_t j = 0; j < grid_weights.size(); ++j)
      log10_bfs[config_idx][j] = atof(tokens[3+j].c_str());
  }
  
  gene.snps.push_back(Snp(curr_snp, log10_bfs));
  genes.push_back(gene);
  
  for(size_t i = 0; i < genes.size(); ++i){
    genes[i].avg_raw_bfs(grid_weights, config_weights);
    if(find(quantities_to_save.begin(), quantities_to_save.end(), "post")
       != quantities_to_save.end()){
      if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "a")
	 != post_probas_to_save.end())
	genes[i].calc_posterior(pi0);
      if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "b")
	 != post_probas_to_save.end())
	genes[i].calc_snp_posteriors_the_eQTL();
      if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "c")
	 != post_probas_to_save.end())
	genes[i].calc_snp_posteriors_an_eQTL();
      if(save_configs)
	genes[i].calc_snp_posteriors_config(config_weights);
      if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "d")
	 != post_probas_to_save.end()){
	if(! save_configs)
	  genes[i].calc_snp_posteriors_config(config_weights);
	genes[i].calc_snp_posteriors_subgroup(config_names, config_weights,
					      config2subgroups);
      }
    }
  }
}

void saveAvgBFs(const vector<Gene> & genes,
		const vector<string> & config_names,
		const vector<string> & quantities_to_save,
		const vector<string> & post_probas_to_save,
		const bool & save_configs,
		const string & file_hm,
		gzFile & stream_hm,
		stringstream & txt,
		const size_t & nb_lines_hm)
{
  size_t nb_subgroups = (size_t) log2(config_names.size() + 1);
  
  for(size_t i = 0; i < genes.size(); ++i){
    for(size_t j = 0; j < genes[i].snps.size(); ++j){
      txt.str("");
      txt << setprecision(4) << scientific
	  << genes[i].name
	  << "\t" << genes[i].snps[j].name;
      if(find(quantities_to_save.begin(), quantities_to_save.end(), "bf")
      	 != quantities_to_save.end()){
      	txt << "\t" << genes[i].log10_bf
      	    << "\t" << genes[i].snps[j].log10_bf;
      	if(save_configs){
      	  for(size_t k = 0; k < genes[i].snps[j].config_log10_bfs.size(); ++k)
      	    txt << "\t" << genes[i].snps[j].config_log10_bfs[k];
      	}
      }
      if(find(quantities_to_save.begin(), quantities_to_save.end(), "post")
      	 != quantities_to_save.end()){
      	if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "a")
      	   != post_probas_to_save.end())
      	  txt << "\t" << genes[i].post;
      	if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "b")
      	   != post_probas_to_save.end())
      	  txt << "\t" << genes[i].snps[j].post_the_eQTL;
      	if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "c")
      	   != post_probas_to_save.end())
      	  txt << "\t" << genes[i].snps[j].post_an_eQTL;
      	if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "d")
      	   != post_probas_to_save.end()){
      	  for(size_t k = 0; k < nb_subgroups; ++k)
      	    txt << "\t" << genes[i].snps[j].post_subgroups[k];
      	}
      	if(save_configs){
      	  for(size_t k = 0; k < genes[i].snps[j].post_configs.size(); ++k)
      	    txt << "\t" << genes[i].snps[j].post_configs[k];
      	}
      }
      txt << "\t" << config_names[genes[i].snps[j].best_config_idx];
      txt << "\n";
      gzwriteLine(stream_hm, txt.str(), file_hm, nb_lines_hm);
    }
  }
}

void averageRawBFsOverGridAndConfig(const vector<string> & files_bf_out,
				    const vector<double> & grid_weights,
				    const vector<string> & config_names,
				    const vector<double> & config_weights,
				    const vector<vector<string> > & config2subgroups,
				    const vector<string> & quantities_to_save,
				    const double & pi0,
				    const vector<string> & post_probas_to_save,
				    const bool & save_configs,
				    const string & file_hm,
				    const int & verbose)
{
  if(verbose > 0)
    cout << "average raw BFs over grid and config ..." << endl;
  
  gzFile stream_hm;
  openFile(file_hm, stream_hm, "wb");
  stringstream txt;
  size_t nb_lines_hm = 0;
  
  size_t nb_subgroups = (size_t) log2(config_weights.size() + 1);
  
  // write header line
  txt << "gene\tsnp";
  if(find(quantities_to_save.begin(), quantities_to_save.end(), "bf")
     != quantities_to_save.end()){
    txt << "\tgene.log10.bf\tsnp.log10.bf";
    if(save_configs){
      for(size_t i = 0; i < config_names.size(); ++i)
	txt << "\tlog10.bf." << config_names[i];
    }
  }
  if(find(quantities_to_save.begin(), quantities_to_save.end(), "post")
     != quantities_to_save.end()){
    if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "a")
       != post_probas_to_save.end())
      txt << "\tgene.post";
    if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "b")
       != post_probas_to_save.end())
      txt << "\tsnp.post.the";
    if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "c")
       != post_probas_to_save.end())
      txt << "\tsnp.post.an";
    if(find(post_probas_to_save.begin(), post_probas_to_save.end(), "d")
       != post_probas_to_save.end()){
      for(size_t i = 0; i < nb_subgroups; ++i)
	txt << "\tsnp.post.s" << i+1;
    }
    if(save_configs){
      for(size_t i = 0; i < config_names.size(); ++i)
	txt << "\tpost." << config_names[i];
    }
  }
  txt << "\tbest.config" << endl;
  nb_lines_hm = 1;
  gzwriteLine(stream_hm, txt.str(), file_hm, nb_lines_hm);
  
  // read each input file, process and save
  vector<string> lines;
  vector<Gene> genes;
  for(size_t f = 0; f < files_bf_out.size(); ++f){
    if(verbose > 0)
      progressBar("", f+1, files_bf_out.size());
    readFile(files_bf_out[f], lines);
    averageBFs(files_bf_out[f], lines, grid_weights, config_names,
	       config_weights, config2subgroups, quantities_to_save,
	       pi0, post_probas_to_save, save_configs, genes);
    saveAvgBFs(genes, config_names, quantities_to_save,
  	       post_probas_to_save, save_configs, file_hm,
  	       stream_hm, txt, nb_lines_hm);
    lines.clear();
    genes.clear();
  }
  if(verbose > 0)
    cerr << endl << flush;
  
  closeFile(file_hm, stream_hm);
}

void run(const string & file_pattern,
	 const string & file_grid_weights,
	 const string & file_config_weights,
	 const vector<string> & quantities_to_save,
	 const double & pi0,
	 const vector<string> & post_probas_to_save,
	 const bool & save_configs,
	 const string & file_hm,
	 const int & verbose)
{
  if(verbose > 0 && !isNan(pi0))
    cout << "pi0: " << setprecision(4) << scientific << pi0 << endl;
  
  vector<double> grid_weights;
  loadFileGridWeights(file_grid_weights, verbose, grid_weights);
  
  vector<string> config_names;
  vector<double> config_weights;
  vector<vector<string> > config2subgroups;
  loadFileConfigWeights(file_config_weights, verbose,
			config_names, config_weights,
			config2subgroups);
  
  vector<string> files_bf_out = glob(file_pattern);
  if(verbose > 0)
    cerr << "nb of files with raw BFs: " << files_bf_out.size()
	 << endl << flush;
  
  if(file_config_weights.empty()) // reduce input for 'eqtlbma_hm'
    averageRawBFsOverGrid(files_bf_out, grid_weights, file_hm, verbose);
  else
    averageRawBFsOverGridAndConfig(files_bf_out, grid_weights,
				   config_names, config_weights,
				   config2subgroups,
				   quantities_to_save, pi0,
				   post_probas_to_save, save_configs,
				   file_hm, verbose);
}

int main(int argc, char ** argv)
{
  int verbose = 1;
  string file_pattern, file_grid_weights, file_config_weights, file_hm;
  double pi0 = NaN;
  vector<string> quantities_to_save, post_probas_to_save;
  bool save_configs = false;
  
  parseCmdLine(argc, argv, verbose, file_pattern, file_grid_weights,
	       file_config_weights, quantities_to_save, pi0,
	       post_probas_to_save, save_configs, file_hm);
  
  time_t time_start, time_end;
  if(verbose > 0){
    time(&time_start);
    cout << "START " << basename(argv[0])
	 << " " << getDateTime(time_start) << endl
	 << "version " << VERSION << " compiled " << __DATE__
	 << " " << __TIME__ << endl
	 << "cmd-line: " << getCmdLine(argc, argv) << endl
	 << "cwd: " << getCurrentDirectory() << endl << flush;
  }
  
  run(file_pattern, file_grid_weights, file_config_weights,
      quantities_to_save, pi0, post_probas_to_save, save_configs,
      file_hm, verbose);
  
  if(verbose > 0){
    time (&time_end);
    cout << "END " << basename(argv[0])
	 << " " << getDateTime(time_end) << endl
	 << "elapsed -> " << getElapsedTime(time_start, time_end) << endl
	 << "max.mem -> " << getMaxMemUsedByProcess2Str() << endl;
  }
  
  return EXIT_SUCCESS;
}