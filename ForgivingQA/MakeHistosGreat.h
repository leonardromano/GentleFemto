/*
 * MakeHistosGreat.h
 *
 *  Created on: Feb 8, 2019
 *      Author: schmollweger
 */

#ifndef FORGIVINGQA_MAKEHISTOSGREAT_H_
#define FORGIVINGQA_MAKEHISTOSGREAT_H_
#include "TH1.h"
#include "TH2.h"
#include "TPad.h"
#include "ForgivingFitter.h"
#include <vector>
class MakeHistosGreat {
 public:
  MakeHistosGreat();
  virtual ~MakeHistosGreat();
  void FormatHistogram(TH1* hist, unsigned int marker, unsigned int color,
                       float size = 1);
  void FormatSmallHistogram(TH1* hist, unsigned int marker, unsigned int color,
                            float size = 1);
  void FormatHistogram(TH2 *histo);
  void DrawAndStore(std::vector<TH1*> hist, const char* outname,
                    const char* drawOption = "");
  void DrawOnPad(std::vector<TH1*> hist, TPad* TPain, const char* drawOption =
                     "");
  void DrawLogYAndStore(std::vector<TH1*> hist, const char* outname,
                        const char* drawOption = "");
  void DrawAndStore(std::vector<TH2*> hist, const char* outname,
                    const char* drawOption = "");
  void DrawLogZAndStore(std::vector<TH2*> hist, const char* outname,
                        const char* drawOption = "");
  static void SetStyle(bool title = true);
  void SetTightMargin(bool set = false) {
    fTightMargin = set;
  }
  ;
  void DrawLatexLabel(float pTMin, float pTMax, ForgivingFitter* fit, TPad* pad,
                      const char* part, float xPos, float yPos);
  void DrawLine(TPad* pad, float xMin, float xMax, float yMin, float yMax);
 private:
  bool fTightMargin;
};

#endif /* FORGIVINGQA_MAKEHISTOSGREAT_H_ */
