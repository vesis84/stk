/***************************************************************************
 *   copyright           : (C) 2004-2005 by Lukas Burget,UPGM,FIT,VUT,Brno *
 *   email               : burget@fit.vutbr.cz                             *
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define SVN_DATE       "$Date$" #define SVN_AUTHOR     "$Author$"
#define SVN_REVISION   "$Revision$"
#define SVN_ID         "$Id$"

//#define MODULE_VERSION "0.2 "__TIME__" "__DATE__
#define MODULE_VERSION "2.0.7 "__TIME__" "__DATE__" "SVN_ID  

#include "STKLib/Decoder.h"
#include "STKLib/labels.h"
#include "STKLib/common.h"
#include "STKLib/Error.h"

#ifndef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  include <getopt.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#define SIGNIFICANT_PROB_DIFFERENCE (0.01)
using namespace STK;

void usage(char *progname)
{
  char *tchrptr;
  if ((tchrptr = strrchr(progname, '\\')) != NULL) progname = tchrptr+1;
  if ((tchrptr = strrchr(progname, '/')) != NULL) progname = tchrptr+1;
  fprintf(stderr,
"\n%s version " MODULE_VERSION "\n"
"\nUSAGE: %s [options] NetworkFiles...\n\n"
" Option                                                     Default\n\n"
//" -e         Expand monophones to triphones                  Off\n"
" -i s       Output network to MNF s                         Off\n"
" -l s       Dir to store network files                      Label file dir\n"
//"*-m         Expansion respects pronuncioation variants      Off\n"
//" -n f       Output list of all CD model to f                Off\n"
//" -p s       Keep phonem s as context independent            \n"
" -q s       Output network formating JMRVWXalpstv           tvl\n"
//"*-s         Strict time optimization                        Off\n"
//" -t s       Treat s as tee (sp) model                       \n"
//"*-u         Turn off network optimization                   Optimize\n"
//" -w         Remove expanded word nodes                      Off\n"
" -y s       Output network file extension                   net\n"
" -A         Print command line arguments                    Off\n"
" -C cf      Set config file to cf                           Default\n"
" -D         Display configuration variables                 off\n"
" -G fmt     Set source trascription format to fmt           As config\n"
" -I mlf     Load master label file mlf                      \n"
" -L dir     Set input label or network dir                  Current\n"
" -P         Set target network format to fmt                STK\n"
" -S f       Set script file to f                            None\n"
" -T N       Set trace flags to N                            0\n"
" -V         Print version information                       Off\n"
" -X ext     Set input label file ext                        lab\n"
"\n"
" %s is Copyright (C) 2004-2005 Lukas Burget et al. and\n"
" licensed under the GNU General Public License, version 2.\n"
" Bug reports, feedback, etc, to: burget@fit.vutbr.cz\n"
"\n", progname, progname, progname);
  exit(-1);
}

#define SNAME "SEXPAND"
const char *optionStr =
" -i r   TARGETMLF"
" -l r   TARGETTRANSCDIR"
//" -m n   RESPECTPRONVARS=TRUE"
//" -n r   TARGETHMMLIST"
//" -p r   CIMODELS"
" -q r   NETFORMATING"
//" -s n   EXACTTIMEMERGE=TRUE"
//" -t r   TEEMODELS"
//" -u n   MINIMIZENET=false"
//" -w n   REMEXPWRDNODES=TRUE"
" -y r   TARGETTRANSCEXT"
" -D n   PRINTCONFIG=TRUE"
" -G r   SOURCETRANSCFMT"
" -I r   SOURCEMLF"
" -L r   SOURCETRANSCDIR"
" -P r   TARGETTRANSCFMT"
" -S l   SCRIPT"
" -T r   TRACE"
" -V n   PRINTVERSION=TRUE"
" -X r   SOURCETRANSCEXT";


int main(int argc, char *argv[]) 
{
  FILE *sfp = NULL;
  ENTRY e, *ep;
  int  i;
  Label *labels;
  char line[1024];
  char label_file[1024];
  MyHSearchData phoneHash, dictHash = {0};
  MyHSearchData triphHash, nonCDphHash, silHash, cfgHash ;

  FileListElem *feature_files=NULL;
  int nfeature_files = 0;
  FileListElem *file_name   = NULL;
  FileListElem **last_file  = &feature_files;

  FILE *out_MLF_fp          = NULL;
  FILE *in_MLF_fp           = NULL;
  double     poster_prune;
  double     grammar_scale;
  double     word_penalty;
  double     model_penalty;
  double     posterior_scale;
  bool       viterbi_posters;
  bool       viterbi_decode;
  const char *cchrptr;
  const char *in_MLF_fn;
  const char *in_lbl_dir;
  const char *in_lbl_ext;
  const char *out_MLF_fn;
  const char *out_lbl_dir;
  const char *out_lbl_ext;
  const char *cd_list_file;
  const char *dictionary;
  const char *label_filter;
  const char *net_filter;
  char *script;
  char *ci_phn;
  char *sil_phn;
  char *tee_phn;
  int trace_flag       =  0;
  int fcnt             = 0;
  
  bool topological_sort = false;

  enum TranscriptionFormat {
    TF_MLF, TF_MNF, TF_HTK, TF_STK, TF_ERR,
    TF_NOF, TF_MOF, //obsolote formats
  } in_transc_fmt = TF_STK, out_transc_fmt = TF_STK;
  int notInDictAction = 0;
  ExpansionOptions expOptions = {0};
  STKNetworkOutputFormat  in_net_fmt = {0};
  STKNetworkOutputFormat out_net_fmt = {0};
  PhoneCorrectnessApproximationType corr_approx;
  
  InitLogMath();
//  LabelFormat out_lbl_fmt = {0};
//  LabelFormat in_lbl_fmt = {0};

  if (argc == 1) usage(argv[0]);

  if (!my_hcreate_r(10,   &nonCDphHash)
  ||  !my_hcreate_r(10,   &silHash)
  ||  !my_hcreate_r(100,  &dictHash)
  ||  !my_hcreate_r(100,  &phoneHash)
  ||  !my_hcreate_r(1000, &triphHash)
  ||  !my_hcreate_r(100,  &cfgHash)) {

  }
  i = ParseOptions(argc, argv, optionStr, SNAME, &cfgHash);
//  htk_compat = GetParamBool(&cfgHash, SNAME":HTKCOMPAT", false);
  for (; i < argc; i++) {
    last_file = AddFileElem(last_file, argv[i]);
    nfeature_files++;
  }
  expOptions.mCDPhoneExpansion =
                 GetParamBool(&cfgHash,SNAME":ALLOWXWRDEXP",    false);
  expOptions.mRespectPronunVar
               = GetParamBool(&cfgHash,SNAME":RESPECTPRONVARS", false);
  expOptions.mStrictTiming
               = GetParamBool(&cfgHash,SNAME":EXACTTIMEMERGE",  false);
  expOptions.mNoWeightPushing
               =!GetParamBool(&cfgHash,SNAME":WEIGHTPUSHING",   true);
  expOptions.mNoOptimization
               =!GetParamBool(&cfgHash,SNAME":MINIMIZENET",     false);
  expOptions.mRemoveNulls
               = GetParamBool(&cfgHash,SNAME":REMOVENULLNODES", false);
  expOptions.mRemoveWordsNodes
               = GetParamBool(&cfgHash,SNAME":REMEXPWRDNODES",  false);
//  in_lbl_fmt.left_extent  = -100 * (long long) (0.5 + 1e5 *
  in_net_fmt.mStartTimeShift = 
                 GetParamFlt(&cfgHash, SNAME":STARTTIMESHIFT",  0.0);
//  in_lbl_fmt.right_extent =  100 * (long long) (0.5 + 1e5 *
  in_net_fmt.mEndTimeShift = 
                 GetParamFlt(&cfgHash, SNAME":ENDTIMESHIFT",    0.0);
  viterbi_decode=GetParamBool(&cfgHash,SNAME":VITERBIDECODE",  false);
  label_filter = GetParamStr(&cfgHash, SNAME":HLABELFILTER",    NULL);
  gpFilterWldcrd= GetParamStr(&cfgHash, SNAME":HFILTERWILDCARD", "$");
  gpScriptFilter= GetParamStr(&cfgHash, SNAME":HSCRIPTFILTER",  NULL);
  net_filter   = GetParamStr(&cfgHash, SNAME":HNETFILTER",      NULL);
  dict_filter  = GetParamStr(&cfgHash, SNAME":HDICTFILTER",     NULL);
//label_ofilter= GetParamStr(&cfgHash, SNAME":HLABELOFILTER",   NULL);
  transc_ofilter=GetParamStr(&cfgHash, SNAME":HNETOFILTER",     NULL);
  dictionary   = GetParamStr(&cfgHash, SNAME":SOURCEDICT",      NULL);
//  hmm_list     = GetParamStr(&cfgHash, SNAME":SOURCEHMMLIST",   NULL);
  in_MLF_fn    = GetParamStr(&cfgHash, SNAME":SOURCEMLF",       NULL);
  in_lbl_dir   = GetParamStr(&cfgHash, SNAME":SOURCETRANSCDIR", NULL);
  in_lbl_ext   = GetParamStr(&cfgHash, SNAME":SOURCETRANSCEXT", NULL);
  out_MLF_fn   = GetParamStr(&cfgHash, SNAME":TARGETMLF",       NULL);
  out_lbl_dir  = GetParamStr(&cfgHash, SNAME":TARGETTRANSCDIR", NULL);
  out_lbl_ext  = GetParamStr(&cfgHash, SNAME":TARGETTRANSCEXT", viterbi_decode ? "rec" : "net");
  trace_flag   = GetParamInt(&cfgHash, SNAME":TRACE",           0);
  cd_list_file = GetParamStr(&cfgHash, SNAME":TARGETHMMLIST",   NULL);
  ci_phn =(char*)GetParamStr(&cfgHash, SNAME":CIMODEL",         NULL);
  tee_phn=(char*)GetParamStr(&cfgHash, SNAME":TEEMODEL",        NULL);
  sil_phn=(char*)GetParamStr(&cfgHash, SNAME":SILMODEL",        NULL);
  script =(char*)GetParamStr(&cfgHash, SNAME":SCRIPT",          NULL);
  poster_prune = GetParamFlt(&cfgHash, SNAME":POSTERIORPRUNING",0.0);
  grammar_scale= GetParamFlt(&cfgHash, SNAME":LMSCALE",         1.0);
  posterior_scale=GetParamFlt(&cfgHash, SNAME":POSTERIORSCALE",  1.0);
  viterbi_posters=GetParamBool(&cfgHash,SNAME":VITERBIPOSTERIORS", false);
  viterbi_decode=GetParamBool(&cfgHash,SNAME":VITERBIDECODE",  false);
  word_penalty = GetParamFlt(&cfgHash, SNAME":WORDPENALTY",     0.0);
  model_penalty= GetParamFlt(&cfgHash, SNAME":MODELPENALTY",    0.0);
  topological_sort=GetParamBool(&cfgHash,SNAME":TOPOLOGICALSORT", false);
  
  expOptions.mTraceFlag = trace_flag;

  bool print_all_options = GetParamBool(&cfgHash,SNAME":PRINTALLOPTIONS", false);

  cchrptr      = GetParamStr(&cfgHash, SNAME":NETFORMATING",  "");
  if (*cchrptr) {
    out_net_fmt.mNoLMLikes    = 1;
    out_net_fmt.mNoTimes       = 1;
    out_net_fmt.mNoPronunVars = 1;
    out_net_fmt.mNoAcousticLikes   = 1;
  }
  while (*cchrptr) {
    switch (*cchrptr++) {
      case 'R': out_net_fmt.mBase62Labels  = 1; // reticent
                out_net_fmt.mLinNodeSeqs   = 1;
                out_net_fmt.mNoDefaults    = 1; break;
      case 'V': out_net_fmt.mArcDefsWithJ  = 1;
                out_net_fmt.mAllFieldNames = 1; break;
      case 'J': out_net_fmt.mArcDefsToEnd  = 1; break;
      case 'W': out_net_fmt.mNoWordNodes   = 1; break;
      case 'M': out_net_fmt.mNoModelNodes  = 1; break;
      case 'X': out_net_fmt.mStripTriphones= 1; break;
      case 't': out_net_fmt.mNoTimes       = 0; break;
      case 's': out_net_fmt.mStartTimes    = 1; break;
      case 'v': out_net_fmt.mNoPronunVars  = 0; break;
      case 'a': out_net_fmt.mNoAcousticLikes=0; break;
      case 'l': out_net_fmt.mNoLMLikes     = 0; break;
      case 'p': out_net_fmt.mAproxAccuracy = 1; break;
      case 'P': out_net_fmt.mPosteriors    = 1; break;
      case 'T': out_net_fmt.mDeriveTimes   = 1; break;
      default:
        Warning("Unknown net formating flag '%c' ignored (JMRVWXalpstv)", *cchrptr);
    }
  }
  in_transc_fmt= (TranscriptionFormat) GetParamEnum(&cfgHash,SNAME":SOURCETRANSCFMT", TF_STK,
                              "HTK", TF_HTK, "STK", TF_STK, "net", TF_NOF, NULL);

  out_transc_fmt= (TranscriptionFormat) GetParamEnum(&cfgHash,SNAME":TARGETTRANSCFMT", viterbi_decode ? TF_HTK : TF_STK,
                              "STK", TF_STK, "net", TF_NOF, viterbi_decode ? "HTK" : NULL, TF_HTK, NULL);

  corr_approx = (PhoneCorrectnessApproximationType) GetParamEnum(&cfgHash,SNAME":MPECORAPPROX", MPE_ApproximateAccuracy,
                              "APPROXACC", MPE_ApproximateAccuracy, "FRAMEACC", MPE_FrameAccuracy, "FRAMEERR", MPE_FrameError, NULL);

  if (GetParamBool(&cfgHash, SNAME":PRINTCONFIG", false)) {
    PrintConfig(&cfgHash);
  }
  
  if (GetParamBool(&cfgHash, SNAME":PRINTVERSION", false)) {
    puts("Version: "MODULE_VERSION"\n");
  }
  
  if (!GetParamBool(&cfgHash,SNAME":ACCEPTUNUSEDPARAM", false)) {
    CheckCommandLineParamUse(&cfgHash);
  }

  if (print_all_options) 
  {
    print_registered_parameters();
  }
  
  if (NULL != script)
  {
    for (script=strtok(script, ","); script != NULL; script=strtok(NULL, ",")) {
      if ((sfp = my_fopen(script, "rt", gpScriptFilter)) == NULL) {
        Error("Cannot open script file %s", script);
      }
      while (fscanf(sfp, "%s", line) == 1) {
        last_file = AddFileElem(last_file, line);
        nfeature_files++;
      }
      my_fclose(sfp);
    }
  }
  
  if (NULL != ci_phn)
  {
    for ( ci_phn=strtok(ci_phn, ",");  ci_phn != NULL; ci_phn=strtok(NULL, ",")) 
    {
      e.key = ci_phn;
      my_hsearch_r(e, FIND, &ep, &nonCDphHash);
      if (ep != NULL) continue;
      if ((e.key = strdup(ci_phn)) == NULL) Error("Insufficient memory");
      e.data = (void *) 0;
      my_hsearch_r(e, ENTER, &ep, &nonCDphHash);
    }
  }

  if (NULL != sil_phn)
  {
    for ( sil_phn=strtok(sil_phn, ",");  sil_phn != NULL; sil_phn=strtok(NULL, ",")) 
    {
      e.key = sil_phn;
      my_hsearch_r(e, FIND, &ep, &silHash);
      if (ep != NULL) continue;
      if ((e.key = strdup(sil_phn)) == NULL) Error("Insufficient memory");
      my_hsearch_r(e, ENTER, &ep, &silHash);
    }
  }
  
  if (NULL != tee_phn)
  {
    for (tee_phn=strtok(tee_phn, ",");tee_phn != NULL;tee_phn=strtok(NULL, ",")) 
    {
      e.key = tee_phn;
      my_hsearch_r(e, FIND, &ep, &nonCDphHash);
      if (ep != NULL) {
        ep->data = (void *) 1;
        continue;
      }
      if ((e.key = strdup(tee_phn)) == NULL) Error("Insufficient memory");
      e.data = (void *) 1;
      my_hsearch_r(e, ENTER, &ep, &nonCDphHash);
    }
  }
  
  if (dictionary != NULL) {
    ReadDictionary(dictionary, &dictHash, &phoneHash);
    notInDictAction  = WORD_NOT_IN_DIC_WARN;
    if (expOptions.mRespectPronunVar) {
      notInDictAction |= (int) PRON_NOT_IN_DIC_ERROR;
    }
  }
  if (dictHash.mNEntries == 0) expOptions.mNoWordExpansion = 1;

  transc_filter = transc_filter != NULL ? transc_filter :
                  in_transc_fmt == TF_STK      ? net_filter    :
                                          label_filter;

  in_MLF_fp  = OpenInputMLF(in_MLF_fn);
  out_MLF_fp = OpenOutputMLF(out_MLF_fn);

  for (file_name=feature_files; file_name != NULL; file_name=file_name->mpNext) 
  {
    if (trace_flag & 1) 
    {
      TraceLog("Processing file %d/%d '%s'", ++fcnt, nfeature_files,
          file_name->logical);
    }

    strcpy(label_file, file_name->logical);
    in_MLF_fp = OpenInputLabelFile(label_file, in_lbl_dir,
                                   in_lbl_ext, in_MLF_fp, in_MLF_fn);

    if (in_MLF_fn == NULL && IsMLF(in_MLF_fp)) 
    {
      in_transc_fmt = in_transc_fmt == TF_HTK ? TF_MLF :
      in_transc_fmt == TF_STK ? TF_MNF :
      in_transc_fmt == TF_NOF ? TF_MOF : TF_ERR;

      in_MLF_fn = label_file;
      assert(in_transc_fmt != TF_ERR);
    }

    for (;;) 
    { //in cases of MLF or MNF, we must process all records
      DecoderNetwork::NodeType*    p_node = NULL;
      DecoderNetwork           my_net(p_node);

      if (in_transc_fmt == TF_MLF
      ||  in_transc_fmt == TF_MNF
      ||  in_transc_fmt == TF_MOF) 
      {
        label_file[0]='\0'; // Ref. MLF is read sequentially record by record

        if (!OpenInputLabelFile(label_file, NULL, NULL, in_MLF_fp, in_MLF_fn)) 
        {
          break; // Whole MLF or MNF processed
        }

        if (trace_flag & 1) TraceLog("Processing file %d '%s'", ++fcnt, label_file);
      }

      if (in_transc_fmt == TF_HTK || in_transc_fmt == TF_MLF) 
      {
        labels = ReadLabels(in_MLF_fp, dictionary ? &dictHash : &phoneHash,
            dictionary ? UL_ERROR : UL_INSERT, in_net_fmt, /*sampleRate*/ 1,
            label_file, in_MLF_fn, NULL);
        
        my_net.BuildFromLabels(labels, dictionary ? NT_WORD : NT_PHONE);

        ReleaseLabels(labels);
      } 
      else if (in_transc_fmt == TF_STK || in_transc_fmt == TF_MNF) 
      {
        ReadSTKNetwork(in_MLF_fp, &dictHash, &phoneHash, notInDictAction, 
            in_net_fmt, /*sampleRate*/ 1, label_file, in_MLF_fn, false,
            my_net); 
      } 
      else if (in_transc_fmt == TF_NOF || in_transc_fmt == TF_MOF) 
      {                                                            
        ReadSTKNetworkInOldFormat(in_MLF_fp, &dictHash, &phoneHash, 
            in_net_fmt, /*sampleRate*/ 1, label_file, in_MLF_fn,
            my_net);
      }

      CloseInputLabelFile(in_MLF_fp, in_MLF_fn);
      
      my_net.ExpansionsAndOptimizations(expOptions, out_net_fmt, &dictHash, 
          &nonCDphHash, &triphHash,
          word_penalty,
          model_penalty,
          grammar_scale,
          posterior_scale);

      if (topological_sort) {
        my_net.SortNodes();
      }
	
      if(poster_prune > 0.0 || out_net_fmt.mPosteriors) {
        // Just to ensure topological ordering
        //my_net.SelfLinksToNullNodes(); // not sure wheather needed
    
        my_net.TopologicalSort();
        my_net.ForwardBackward(word_penalty, model_penalty, grammar_scale, posterior_scale, viterbi_posters);
	if(poster_prune > 0.0) {
          my_net.PosteriorPrune(poster_prune, word_penalty, model_penalty, grammar_scale, posterior_scale);
	}
      }


      if (out_net_fmt.mAproxAccuracy)
        my_net.ComputePhoneCorrectnes(corr_approx, &silHash);
	
      if (viterbi_decode) {
        Decoder<DecoderNetwork> decoder;
        DecoderNetwork *tmp_net = &decoder.rNetwork();
        decoder.rNetwork() = my_net;	
	decoder.Init(NULL, NULL);

//        decoder.mTimePruning       = false;
        decoder.mWPenalty          = word_penalty;
        decoder.mMPenalty          = model_penalty;
        decoder.mLmScale           = grammar_scale;
//      decoder.mPronScale         = pronun_scale;  // Ignored by posterior pruning, so not yet implementet here too
        decoder.mAlignment         = WORD_ALIGNMENT | MODEL_ALIGNMENT;
        decoder.mPruningThresh     = -LOG_0;
    
        decoder.ViterbiInit();
	FLOAT like = decoder.ViterbiDone(&labels, NULL, true);
	decoder.rNetwork() = *tmp_net;
	
	if (labels) {
          Label* label;
          for (label = labels; label->mpNextLevel != NULL; label = label->mpNextLevel) {}
          for (; label != NULL; label = label->mpNext) {
            if(label->mpName != NULL)
              fprintf(stdout, "%s ", label->mpName);
          }
          TraceLog(" == %f", like);
        }
        out_MLF_fp = OpenOutputLabelFile(label_file, out_lbl_dir, out_lbl_ext,
                                         out_MLF_fp, out_MLF_fn);

        if (out_transc_fmt == TF_HTK) {
          WriteLabels(out_MLF_fp, labels, out_net_fmt, 1, label_file, out_MLF_fn);
        } else {
        // create temporary linear network from labels, just to store it
          DecoderNetwork tmp_net(labels, NT_PHONE);
          WriteSTKNetwork(out_MLF_fp, tmp_net, out_net_fmt, 1, label_file, out_MLF_fn,
                          word_penalty, model_penalty, grammar_scale, posterior_scale);
        }
	ReleaseLabels(labels);
      } else {
        out_MLF_fp = OpenOutputLabelFile(label_file, out_lbl_dir, out_lbl_ext,
                                      out_MLF_fp, out_MLF_fn);
				
        if (out_transc_fmt == TF_NOF) {
          WriteSTKNetworkInOldFormat(out_MLF_fp, my_net, out_net_fmt, 1, label_file, out_MLF_fn);
        } else {
          WriteSTKNetwork(out_MLF_fp, my_net, out_net_fmt, 1, label_file, out_MLF_fn,
                          word_penalty, model_penalty, grammar_scale, posterior_scale);
        }
      }

      if(poster_prune > 0.0 || out_net_fmt.mPosteriors) {  
        my_net.FreePosteriors();
      }


      CloseOutputLabelFile(out_MLF_fp, out_MLF_fn);
      my_net.Clear();

      //FreeNetwork(p_node);

      if (in_transc_fmt == TF_HTK
      || in_transc_fmt == TF_STK
      || in_transc_fmt == TF_NOF) 
      {
        // We are dealing with single record in these cases
        break; 
      }
    }

    if (in_transc_fmt == TF_MLF || in_transc_fmt == TF_MNF) {
      CloseInputMLF(in_MLF_fp);
    }
  }
  if (out_MLF_fn != NULL) fclose(out_MLF_fp);

  if (in_MLF_fn != NULL && (in_transc_fmt == TF_HTK || in_transc_fmt == TF_STK)) {
    CloseInputMLF(in_MLF_fp);
  }
  if (cd_list_file != NULL) {
    FILE *fp = fopen(cd_list_file, "wt");
    if (fp == NULL) Error("Cannot open output file: '%s'", cd_list_file);

    for (size_t i = 0; i < triphHash.mNEntries; i++) {
      if (fprintf(fp, "%s\n", (char *) triphHash.mpEntry[i]->key) < 0) {
        Error("Cannot write to file: '%s'", cd_list_file);
      }
    }
    fclose(fp);
  }
  my_hdestroy_r(&phoneHash, 1);
  my_hdestroy_r(&triphHash, 0);
  my_hdestroy_r(&nonCDphHash, 0);
  FreeDictionary(&dictHash);
  
  for (size_t i = 0; i < cfgHash.mNEntries; i++) 
    free(cfgHash.mpEntry[i]->data);

  while (feature_files) {
    file_name = feature_files;
    feature_files = feature_files->mpNext;
    free(file_name);
  }

  
  my_hdestroy_r(&cfgHash, 1);
  return 0;
}
