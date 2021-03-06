/*
 * DreamCF.cxx
 *
 *  Created on: Aug 22, 2018
 *      Author: hohlweger
 */

#include "DreamCF.h"
#include "TFile.h"
#include "TMath.h"
#include <iostream>

DreamCF::DreamCF()
    : fCF(),
      fRatio(),
      fPairOne(nullptr),
      fPairTwo(nullptr) {

}

DreamCF::~DreamCF() {
  for (auto it : fCF) {
    delete it;
  }
  for (auto it : fRatio) {
    delete it;
  }
  delete fPairOne;
  delete fPairTwo;
}

void DreamCF::GetCorrelations(const char* pairName) {
  if (fPairOne && fPairOne->GetPair()) {
    if (fPairTwo && fPairTwo->GetPair()) {
      TH1F* CFSum = AddCF(fPairOne->GetPair()->GetCF(),
                          fPairTwo->GetPair()->GetCF(),
                          Form("hCkTotNormWeight%s", pairName));
      if (CFSum) {
        fCF.push_back(CFSum);
        TString CFSumMeVName = Form("%sMeV", CFSum->GetName());
        TH1F* CFMeVSum = ConvertToOtherUnit(CFSum, 1000, CFSumMeVName.Data());
        if (CFMeVSum) {
          fCF.push_back(CFMeVSum);
        }
      } else {
        std::cout << "No Pair 2 Set, only setting Pair 1! \n";
        //existence already checked
        fCF.push_back(fPairOne->GetPair()->GetCF());
      }
    }
  } else {
    std::cout << "No Pair 1 Set, only setting Pair 2 \n";
    if (fPairTwo && fPairTwo->GetPair())
      fCF.push_back(fPairTwo->GetPair()->GetCF());
    else
      std::cout << "No Pair 2 set either \n";

  }
  if (fPairOne && fPairTwo) {
    if (fPairOne->GetNDists() == fPairTwo->GetNDists()) {
      LoopCorrelations(fPairOne->GetShiftedEmpty(), fPairTwo->GetShiftedEmpty(),
                       Form("hCk_Shifted%s", pairName));
      LoopCorrelations(fPairOne->GetFixShifted(), fPairTwo->GetFixShifted(),
                       Form("hCk_FixShifted%s", pairName));
      LoopCorrelations(fPairOne->GetRebinned(), fPairTwo->GetRebinned(),
                       Form("hCk_Rebinned%s", pairName));
      LoopCorrelations(fPairOne->GetReweighted(), fPairTwo->GetReweighted(),
                       Form("hCk_Reweighted%s", pairName));
    } else {
      std::cout << "Pair 1 with " << fPairOne->GetNDists()
                << " histograms, Pair 2 with " << fPairTwo->GetNDists()
                << " histograms \n";
    }
  } else if (fPairOne && !fPairTwo) {
    LoopCorrelations(fPairOne->GetShiftedEmpty(),
                     Form("hCk_Shifted%s", pairName));
    LoopCorrelations(fPairOne->GetFixShifted(),
                     Form("hCk_FixShifted%s", pairName));
    LoopCorrelations(fPairOne->GetRebinned(), Form("hCk_Rebinned%s", pairName));
    LoopCorrelations(fPairOne->GetReweighted(),
                     Form("hCk_Reweighted%s", pairName));
  } else if (fPairTwo && !fPairOne) {
    LoopCorrelations(fPairTwo->GetShiftedEmpty(),
                     Form("hCk_Shifted%s", pairName));
    LoopCorrelations(fPairTwo->GetFixShifted(),
                     Form("hCk_FixShifted%s", pairName));
    LoopCorrelations(fPairTwo->GetRebinned(), Form("hCk_Rebinned%s", pairName));
    LoopCorrelations(fPairTwo->GetReweighted(),
                     Form("hCk_Reweighted%s", pairName));
  } else {
    std::cout << "Pair 1 and Pair 2 missing \n";
  }
  return;
}

void DreamCF::LoopCorrelations(std::vector<DreamDist*> PairOne,
                               std::vector<DreamDist*> PairTwo,
                               const char* name) {
  if (PairOne.size() != PairTwo.size()) {
    std::cout << "Different size of pair(" << PairOne.size()
              << ") and antiparticle pair(" << PairTwo.size() << ") ! \n";
  } else {
    unsigned int iIter = 0;
    while (iIter < PairOne.size()) {
      DreamDist* PartPair = PairOne.at(iIter);
      DreamDist* AntiPartPair = PairTwo.at(iIter);
      TString CFSumName = Form("%s_%i", name, iIter);
      TH1F* CFSum = AddCF(PartPair->GetCF(), AntiPartPair->GetCF(),
                          CFSumName.Data());
      if (CFSum) {
        fCF.push_back(CFSum);
        TString CFSumMeVName = Form("%sMeV_%i", name, iIter);
        TH1F* CFMeVSum = ConvertToOtherUnit(CFSum, 1000, CFSumMeVName.Data());
        if (CFMeVSum) {
          fCF.push_back(CFMeVSum);
        }
      } else {
        if (PartPair->GetCF()) {
          std::cout << "For iteration " << iIter << " Particle Pair CF ("
                    << PartPair->GetSEDist()->GetName() << ")is missing \n";
        } else if (AntiPartPair->GetCF()) {
          std::cout << "For iteration " << iIter << " AntiParticle Pair CF ("
                    << AntiPartPair->GetSEDist()->GetName() << ")is missing \n";
        }
      }
      iIter++;
    }
  }
}

void DreamCF::LoopCorrelations(std::vector<DreamDist*> Pair, const char* name) {
  unsigned int iIter = 0;
  for (auto it : Pair) {
    TString CFSumName = Form("%s_%i", name, iIter++);
    TH1F* CFClone = (TH1F*) it->GetCF()->Clone(CFSumName.Data());
    if (CFClone) {
      fCF.push_back(CFClone);
      TString CFSumMeVName = Form("%sMeV_%i", name, iIter);
      TH1F* CFMeVSum = ConvertToOtherUnit(CFClone, 1000, CFSumMeVName.Data());
      if (CFMeVSum) {
        fCF.push_back(CFMeVSum);
      }
    } else {
      std::cout << "For iteration " << iIter << " Particle Pair CF ("
                << it->GetSEDist()->GetName() << ")is missing \n";
    }
  }
}

void DreamCF::WriteOutput(const char* name) {
  TFile* output = TFile::Open(name, "RECREATE");
  WriteOutput(output,true);
}

void DreamCF::WriteOutput(TFile* output, bool closeFile) {
  output->cd();
  for (auto& it : fCF) {
    it->Write();
    delete it;
  }

  for (auto& it : fRatio) {
    it->Write();
    delete it;
  }
  if (fPairOne) {
    TList *PairDist = new TList();
    PairDist->SetOwner();
    PairDist->SetName("PairDist");
    fPairOne->WriteOutput(PairDist);
    PairDist->Write("PairDist", 1);
  } else {
    std::cout << "not writing Pair 1 \n";
  }
  if (fPairTwo) {
    TList *AntiPairDist = new TList();
    AntiPairDist->SetOwner();
    AntiPairDist->SetName("AntiPairDist");
    fPairTwo->WriteOutput(AntiPairDist);
    AntiPairDist->Write("AntiPairDist", 1);
  } else {
    std::cout << "not writing Pair 2 \n";
  }
  if (closeFile)output->Close();
  return;
}

TH1F* DreamCF::AddCF(TH1F* CF1, TH1F* CF2, const char* name) {
  TH1F* hist_CF_sum = nullptr;
  if (CF1 && CF2) {
    if (CF1->GetXaxis()->GetXmin() == CF2->GetXaxis()->GetXmin()) {
      TH1F* Ratio = (TH1F*) CF1->Clone(TString::Format("%sRatio",name));
      Ratio->Divide(CF2);
      fRatio.push_back(Ratio);
      //Calculate CFs with error weighting
      hist_CF_sum = (TH1F*) CF1->Clone(name);

      int NBins = hist_CF_sum->GetNbinsX();

      for (int i = 0; i < NBins; i++) {
        double CF1_val = CF1->GetBinContent(i + 1);
        double CF1_err = CF1->GetBinError(i + 1);
        double CF2_val = CF2->GetBinContent(i + 1);
        double CF2_err = CF2->GetBinError(i + 1);
        //average for bin i:
        if (CF1_val != 0. && CF2_val != 0.) {
          double CF1_err_weight = 1. / TMath::Power(CF1_err, 2.);
          double CF2_err_weight = 1. / TMath::Power(CF2_err, 2.);

          double CF_sum_average = (CF1_err_weight * CF1_val
              + CF2_err_weight * CF2_val) / (CF1_err_weight + CF2_err_weight);
          double CF_sum_err = 1. / TMath::Sqrt(CF1_err_weight + CF2_err_weight);

          hist_CF_sum->SetBinContent(i + 1, CF_sum_average);
          hist_CF_sum->SetBinError(i + 1, CF_sum_err);
        } else if (CF1_val == 0. && CF2_val != 0.) {
          hist_CF_sum->SetBinContent(i + 1, CF2_val);
          hist_CF_sum->SetBinError(i + 1, CF2_err);
        } else if (CF2_val == 0 && CF1_val != 0.) {
          hist_CF_sum->SetBinContent(i + 1, CF1_val);
          hist_CF_sum->SetBinError(i + 1, CF1_err);
        }
      }
    } else {
      std::cout << "Skipping " << CF1->GetName() << " and " << CF2->GetName()
                << " due to uneven beginning of binning ("
                << CF1->GetXaxis()->GetXmin() << " and "
                << CF2->GetXaxis()->GetXmin() << ") \n";
    }
  }
  return hist_CF_sum;
}

TH1F* DreamCF::ConvertToOtherUnit(TH1F* HistCF, int Scale, const char* name) {
  int nBins = HistCF->GetNbinsX();
  float kMin = HistCF->GetXaxis()->GetXmin();
  float kMax = HistCF->GetXaxis()->GetXmax();
  TH1F* HistScaled = new TH1F(name, name, nBins, kMin * Scale, kMax * Scale);
  for (int iBin = 1; iBin <= nBins; ++iBin) {
    HistScaled->SetBinContent(iBin, HistCF->GetBinContent(iBin));
    HistScaled->SetBinError(iBin, HistCF->GetBinError(iBin));
  }
  return HistScaled;
}

TH1F* DreamCF::FindCorrelationFunction(TString name) {
  TH1F* output = nullptr;
  for (auto it : fCF) {
    TString itName = it->GetName();
    if (itName.Contains(name.Data())) {
      std::cout << "For Histo: " << name << '\t' << "we use the "
                << itName.Data() << std::endl;
      output = it;
    }
  }
  if (!output) {
    std::cout << "Output Histogram not found for " << name.Data() << std::endl;
    std::cout << "What we offer is the following: \n";
    for (auto it : fCF) {
      std::cout << it->GetName() << std::endl;
    }
  }
  return output;
}
