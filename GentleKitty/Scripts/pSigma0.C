#include "DLM_Source.h"
#include "DLM_Potentials.h"
#include "DLM_CkModels.h"
#include "DLM_WfModel.h"
#include "CATS.h"
#include "DLM_CkDecomposition.h"
#include "DLM_Fitters.h"
#include "TidyCats.h"
#include "CATSInputSigma0.h"
#include "TRandom3.h"
#include "TFile.h"
#include "TGraph.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TF1.h"
#include "TCanvas.h"
#include <iostream>
#include "CATSLambdaParam.h"
#include "SidebandSigma.h"
#include "TMinuit.h"
#include "TMath.h"
#include "TPaveText.h"
#include "DreamPlot.h"
#include "TNtuple.h"

/// Number of parameters for the sideband fit
const int nSidebandPars = 6;

/// =====================================================================================
/// Fit for the sidebands
auto sidebandFit =
    [ ] (double *x, double *p) {
      return p[0] + p[1] * x[0] + p[2] * x[0] * x[0] + p[3] * x[0] * x[0] *x[0] + std::exp(p[4] + p[5] * x[0]);
    };

/// =====================================================================================
/// Function to cast the nice lambda from above to CATS...
double sidebandFitCATS(const double &Momentum, const double *SourcePar,
                       const double *PotPar) {
  double *x = new double[1];
  x[0] = Momentum;
  double *p = const_cast<double*>(PotPar);
  return sidebandFit(x, p);
}

/// =====================================================================================
void FitSigma0(const unsigned& NumIter, TString InputDir, TString trigger,
               TString suffix, TString OutputDir, const int potential,
               std::vector<double> params) {
  bool batchmode = true;
  double d0, REf0inv, IMf0inv, deltap0, deltap1, deltap2, etap0, etap1, etap2;

  DreamPlot::SetStyle();
  bool fastPlot = (NumIter == 0) ? true : false;
  TRandom3 rangen(0);
  TidyCats* tidy = new TidyCats();  // for some reason we need this for the thing to compile

  int nArguments = 32;
  TString varList =
      TString::Format(
          "IterID:femtoFitRange:BLSlope:ppRadius:bl_a:bl_a_err:bl_b:bl_b_err:"
          "sb_p0:sb_p0_err:sb_p1:sb_p1_err:sb_p2:sb_p2_err:sb_p3:sb_p3_err:sb_p4:sb_p4_err:sb_p5:sb_p5_err:"
          "primaryContrib:fakeContrib:SBnormDown:SBnormUp:"
          "chi2NDFGlobal:pvalGlobal:chi2Local:ndf:chi2NDF:pval:nSigma:CFneg")
          .Data();
  if (potential == 0) {
    varList += ":d0:REf0inv:IMf0inv";
    nArguments += 3;
  } else if (potential == 1) {
    varList += ":deltap0:deltap1:deltap2:etap0:etap1:etap2";
    nArguments += 6;
  }

  TNtuple* ntResult = new TNtuple("fitResult", "fitResult", varList.Data());

  Float_t ntBuffer[nArguments];
  int iterID = 0;
  bool useBaseline = true;

  TString graphfilename;
  if (potential == 0) {
    if (params.size() != 3) {
      std::cout << "ERROR: Wrong number of scattering parameters\n";
      return;
    }
    d0 = params[0];
    REf0inv = params[1];
    IMf0inv = params[2];

    graphfilename = TString::Format("%s/Param_pSigma0_%i_%.3f_%.3f_%.3f.root",
                                    OutputDir.Data(), potential, d0, REf0inv,
                                    IMf0inv);
  } else if (potential == 1) {
    if (params.size() != 6) {
      std::cout << "ERROR: Wrong number of parameters for delta/eta\n";
      return;
    }
    deltap0 = params[0];
    deltap1 = params[1];
    deltap2 = params[2];
    etap0 = params[3];
    etap1 = params[4];
    etap2 = params[5];

    graphfilename = TString::Format(
        "%s/Param_pSigma0_%i_%.1f_%.4f_%.7f_%.2f_%.5f_%.8f.root",
        OutputDir.Data(), potential, deltap0, deltap1, deltap2, etap0, etap1,
        etap2);

  } else {
    graphfilename = TString::Format("%s/Param_pSigma0_%i.root",
                                    OutputDir.Data(), potential);
  }
  auto param = new TFile(graphfilename, "RECREATE");

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// CATS input
  TString CalibBaseDir = "~/cernbox/SystematicsAndCalib/ppRun2_HM/";
  CATSInputSigma0 *CATSinput = new CATSInputSigma0();
  CATSinput->SetCalibBaseDir(CalibBaseDir.Data());
  CATSinput->SetMomResFileName("run2_decay_matrices_old.root");
  CATSinput->ReadResFile();
  CATSinput->SetSigmaFileName("Sample6_MeV_compact.root");
  CATSinput->ReadSigmaFile();
  CATSinput->ReadSigma0CorrelationFile(InputDir.Data(), trigger.Data(),
                                       suffix.Data());
  CATSinput->ObtainCFs(10, 250, 400);
  TString dataHistName = "hCk_ReweightedpSigma0MeV_0";
  auto dataHist = CATSinput->GetCF("pSigma0", dataHistName.Data());
  if (!dataHist) {
    std::cerr << "ERROR pSigma0 fitter: p-Sigma0 histogram missing\n";
    return;
  }
  auto sidebandHistUp = CATSinput->GetCF("pSigmaSBUp",
                                         "hCk_ReweightedpSigmaSBUpMeV_0");
  auto sidebandHistLow = CATSinput->GetCF("pSigmaSBLow",
                                          "hCk_ReweightedpSigmaSBLowMeV_0");
  if (!sidebandHistLow || !sidebandHistUp) {
    std::cerr << "ERROR pSigma0 fitter: p-pSigmaSB histogram missing\n";
    return;
  }

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// Set up the CATS ranges, lambda parameters, etc.
  const int binwidth = 10;
  int NumMomBins_pSigma = int(800 / binwidth);
  double kMin_pSigma = dataHist->GetBinCenter(1) - binwidth / 2.f;
  double kMax_pSigma = kMin_pSigma + binwidth * NumMomBins_pSigma;

  std::cout << "kMin_pSigma: " << kMin_pSigma << std::endl;
  std::cout << "kMax_pSigma: " << kMax_pSigma << std::endl;
  std::cout << "Binwidth: " << binwidth << std::endl;
  std::cout << "NumMomBins_pSigma: " << NumMomBins_pSigma << std::endl;

  // pp radius systematic variations
  const double ppRadius = 1.30796;
  const double ppRadiusStatErr = 0.00437419;
  const double ppRadiusSystErrUp = 0.00385732;
  const double ppRadiusSystErrDown = 0.0123427;
  const double ppRadiusLower = ppRadius
      - std::sqrt(
          ppRadiusStatErr * ppRadiusStatErr
              + ppRadiusSystErrDown * ppRadiusSystErrDown);
  const double ppRadiusUpper = ppRadius
      + std::sqrt(
          ppRadiusStatErr * ppRadiusStatErr
              + ppRadiusSystErrUp * ppRadiusSystErrUp);
  const std::vector<double> sourceSize = { { ppRadius, ppRadiusLower,
      ppRadiusUpper } };

  // femto fit region systematic variations
  const std::vector<double> femtoFitRegionUp = { { 550, 500, 600 } };

  /// Lambda parameters
  const double protonPurity = 0.991213;
  const double protonPrimary = 0.874808;
  const double protonLambda = 0.0876342;

  // proton secondary contribution systematic variations
  const std::vector<double> protonSecondary = { { protonLambda
      / (1. - protonPrimary), protonLambda / (1. - protonPrimary) * 0.8,
      protonLambda / (1. - protonPrimary) * 1.2 } };
  std::vector<CATSLambdaParam> lambdaParams;

  const double sigmaPurity = 0.289;
  const double sigmaPrimary = 1.;
  const Particle sigma0(sigmaPurity, sigmaPrimary, { { 0 } });

  for (size_t lambdaIter = 0; lambdaIter < protonSecondary.size();
      ++lambdaIter) {
    const Particle proton(
        protonPurity,
        protonPrimary,
        { { (1. - protonPrimary) * protonSecondary[lambdaIter], (1.
            - protonPrimary) * (1 - protonSecondary[lambdaIter]) } });

    lambdaParams.push_back( { proton, sigma0 });
  }

  // sideband fit normalization range systematic variation
  const std::vector<double> sidebandNormDown = { { 300, 250, 350 } };
  const std::vector<double> sidebandNormUp = { { 500, 450, 550 } };

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// Fit the sideband
  auto side = new SidebandSigma();
  side->SetRebin(10);
  side->SetSideBandFile(InputDir.Data(), trigger.Data(), suffix.Data());

  /// Prefit for the baseline
  auto Prefit = (TH1F*) dataHist->Clone(Form("%s_prefit", dataHist->GetName()));
  auto funct_0 = new TF1("myPol0", "pol0", 250, 750);
  Prefit->Fit(funct_0, "FSNRMQ");

  TF1* funct_1 = new TF1("myPol1", "pol1", 250, 750);
  Prefit->Fit(funct_1, "FSNRMQ");
  gMinuit->SetErrorDef(1);  // 1 corresponds to 1 sigma contour, 4 to 2 sigma
  auto grPrefitContour = (TGraph*) gMinuit->Contour(40, 0, 1);

  std::vector<double> prefit_a;
  prefit_a.emplace_back(funct_0->GetParameter(0));
  prefit_a.emplace_back(funct_1->GetParameter(0));
  prefit_a.emplace_back(
      TMath::MinElement(grPrefitContour->GetN(), grPrefitContour->GetX()));
  prefit_a.emplace_back(
      TMath::MaxElement(grPrefitContour->GetN(), grPrefitContour->GetX()));

  std::vector<double> prefit_b;
  prefit_b.emplace_back(0);
  prefit_b.emplace_back(funct_1->GetParameter(1));
  prefit_b.emplace_back(grPrefitContour->Eval(prefit_a[2]));
  prefit_b.emplace_back(grPrefitContour->Eval(prefit_a[3]));

  std::cout << "Result of the prefit to constrain the baseline\n";
  for (size_t i = 0; i < prefit_a.size(); ++i) {
    std::cout << i << " a: " << prefit_a[i] << " b: " << prefit_b[i] << "\n";
  }

  delete funct_0;
  delete funct_1;
  delete Prefit;

  if (NumIter != 0) {
    std::cout << "\n\nStarting the systematic variations\n";
    std::cout << "Number of variations of the fit region: "
              << femtoFitRegionUp.size() << "\n";
    std::cout << "Number of variations of the baseline:   " << prefit_a.size()
              << "\n";
    std::cout << "Number of variations of the source size : "
              << sourceSize.size() << "\n";
    std::cout << "Number of variations of the sideband normalization: "
              << sidebandNormDown.size() << "\n";
    std::cout << "Number of variations of the lambda param: "
              << protonSecondary.size() << "\n\n";
  }

  // Set up the model, fitter, etc.
  DLM_Ck* Ck_pSigma0;
  CATS AB_pSigma0;
  if (potential == 0) {  //  Effective Lednicky with scattering parameters
    std::cout << "Running with scattering parameters - d0 = " << d0
              << " fm - Re(f0^-1) = " << REf0inv << " fm^-1 - Im(f0^-1) = "
              << IMf0inv << "\n";
    Ck_pSigma0 = new DLM_Ck(1, 3, NumMomBins_pSigma, kMin_pSigma, kMax_pSigma,
                            ComplexLednicky_Singlet_InvScatLen);
  } else if (potential == 1) {  // Effective Lednicky with delta/eta
    std::cout << "Running with delta/eta parametrization \n";
    std::cout << "Delta: " << deltap0 << " " << deltap1 << " " << deltap2
              << "\n";
    std::cout << "Eta  : " << etap0 << " " << etap1 << " " << etap2 << "\n";
    Ck_pSigma0 = new DLM_Ck(1, 6, NumMomBins_pSigma, kMin_pSigma, kMax_pSigma,
                            LednickySingletScatAmplitude);
  } else if (potential == 2) {  // Lednicky coupled channel model fss2
    std::cout << "Running with coupled Lednicky \n";
    Ck_pSigma0 = new DLM_Ck(1, 0, NumMomBins_pSigma, kMin_pSigma, kMax_pSigma,
                            Lednicky_gauss_Sigma0);
  } else if (potential == 3) {  // Haidenbauer WF
    std::cout << "Running with the Haidenbauer chiEFT potential \n";
    // Haidenbauer is valid up to 350 MeV, therefore we have to adopt
    double kMax_pSigma_Haidenbauer = kMin_pSigma;
    int NumMomBins_pSigma_Haidenbauer = 0;
    while (kMax_pSigma_Haidenbauer < 348 - binwidth) {
      kMax_pSigma_Haidenbauer += binwidth;
      ++NumMomBins_pSigma_Haidenbauer;
    }
    AB_pSigma0.SetMomBins(NumMomBins_pSigma_Haidenbauer, kMin_pSigma,
                          kMax_pSigma_Haidenbauer);
    CATSparameters* cPars = new CATSparameters(CATSparameters::tSource, 1,
                                               true);
    cPars->SetParameter(0, ppRadius);
    AB_pSigma0.SetAnaSource(GaussSource, *cPars);
    AB_pSigma0.SetUseAnalyticSource(true);
    AB_pSigma0.SetMomentumDependentSource(false);
    AB_pSigma0.SetThetaDependentSource(false);
    AB_pSigma0.SetExcludeFailedBins(false);
    DLM_Histo<complex<double>>*** ExternalWF = nullptr;
#ifdef __APPLE__
    ExternalWF = Init_pSigma0_Haidenbauer(
        "/Users/amathis/CERNHome/Sigma0/HaidenbauerWF/", AB_pSigma0);
#else
    ExternalWF = Init_pSigma0_Haidenbauer(
        "/home/amathis/CERNhome/Sigma0/HaidenbauerWF/", AB_pSigma0);
#endif
    for (unsigned uCh = 0; uCh < AB_pSigma0.GetNumChannels(); uCh++) {
      AB_pSigma0.SetExternalWaveFunction(uCh, 0, ExternalWF[0][uCh][0],
                                         ExternalWF[1][uCh][0]);
    }
    AB_pSigma0.KillTheCat();
    Ck_pSigma0 = new DLM_Ck(1, 0, AB_pSigma0);
    CleanUpWfHisto(AB_pSigma0.GetNumChannels(), ExternalWF);
  }

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// "Systematic" variations

  // 1. Femto fit range
  for (size_t femtoFitIter = 0; femtoFitIter < femtoFitRegionUp.size();
      ++femtoFitIter) {

    // 2. Baseline treatment
    for (size_t blIter = 0; blIter < prefit_a.size(); ++blIter) {

      if (blIter == 0) {
        useBaseline = false;  //use baseline
      } else {
        useBaseline = true;  // no baseline
      }

      // 3. Sideband normalization
      for (size_t sbNormIter = 0; sbNormIter < sidebandNormDown.size();
          ++sbNormIter) {

        side->SetNormalizationRange(sidebandNormDown[sbNormIter],
                                    sidebandNormUp[sbNormIter]);

        side->SideBandCFs();
        auto SBmerge = side->GetSideBands(5);
        auto sideband = new TF1(Form("sideband_%i", iterID), sidebandFit, 0,
                                650, nSidebandPars);
        sideband->SetParameter(0, -1.5);
        sideband->SetParameter(1, 0.);
        sideband->SetParameter(2, 0);
        sideband->SetParameter(3, 0.1);
        sideband->SetParameter(4, 1);
        sideband->SetParameter(5, 0);
        SBmerge->Fit(sideband, "FSNRMQ");

        DLM_Ck* Ck_SideBand = new DLM_Ck(0, nSidebandPars, NumMomBins_pSigma,
                                         kMin_pSigma, kMax_pSigma,
                                         sidebandFitCATS);

        for (unsigned i = 0; i < sideband->GetNumberFreeParameters(); ++i) {
          if (!batchmode) {
            std::cout << i << " " << sideband->GetParameter(i) << std::endl;
          }
          Ck_SideBand->SetPotPar(i, sideband->GetParameter(i));
        }
        Ck_SideBand->Update();

        // 4. Source size
        for (size_t sizeIter = 0; sizeIter < sourceSize.size(); ++sizeIter) {

          // 5. Lambda parameters
          for (size_t lambdaIter = 0; lambdaIter < lambdaParams.size();
              ++lambdaIter) {

            if (!batchmode) {
              std::cout << "Processing iteration " << iterID << "\n";
            }

            /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            /// Correlation function
            Ck_pSigma0->SetSourcePar(0, sourceSize[sizeIter]);
            Ck_pSigma0->Update();

            DLM_CkDecomposition CkDec_pSigma0("pSigma0", 1, *Ck_pSigma0,
                                              CATSinput->GetSigmaFile(1));

            /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

            DLM_CkDecomposition CkDec_SideBand("pSigma0SideBand", 0,
                                               *Ck_SideBand, nullptr);
            CkDec_pSigma0.AddContribution(
                0,
                lambdaParams[lambdaIter].GetLambdaParam(
                    CATSLambdaParam::Primary, CATSLambdaParam::Fake, 0, 0),
                DLM_CkDecomposition::cFake, &CkDec_SideBand);
            CkDec_pSigma0.Update();

            /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            /// Fitter
            DLM_Fitter1* fitter = new DLM_Fitter1(1);
            fitter->SetSystem(0, *dataHist, 1, CkDec_pSigma0, kMin_pSigma,
                              femtoFitRegionUp[femtoFitIter], 1000, 1000);
            fitter->SetSeparateBL(0, false);              //Simultaneous BL
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_a, prefit_a[blIter]);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_b, prefit_b[blIter]);
            fitter->AddSameSource("pSigma0SideBand", "pSigma0", 1);

            //Fit BL & Normalization
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_c, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_Cl, -1.);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_sor0,
                                 sourceSize[sizeIter]);

            // Suppress warnings from ROOT
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot0, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot1, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot2, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot3, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot4, 0);
            fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot5, 0);
            if (potential == 0) {
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot0, REf0inv);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot1, IMf0inv);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot2, d0);
            } else if (potential == 1) {
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot0, deltap0);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot1, deltap1);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot2, deltap2);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot3, etap0);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot4, etap1);
              fitter->FixParameter("pSigma0", DLM_Fitter1::p_pot5, etap2);
            }

            fitter->GoBabyGo();

            /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            /// Get the parameters from the fit
            const double bl_a = fitter->GetParameter("pSigma0",
                                                     DLM_Fitter1::p_a);
            const double bl_a_err = fitter->GetParError("pSigma0",
                                                        DLM_Fitter1::p_a);
            const double bl_b = fitter->GetParameter("pSigma0",
                                                     DLM_Fitter1::p_b);
            const double bl_b_err = fitter->GetParError("pSigma0",
                                                        DLM_Fitter1::p_b);
            const double Cl = fitter->GetParameter("pSigma0", DLM_Fitter1::p_c);
            const double chi2 = fitter->GetChi2Ndf();
            const double pval = fitter->GetPval();
            const bool isCFneg = fitter->CheckNegativeCk();

            TGraph FitResult_pSigma0;
            FitResult_pSigma0.SetName(
                TString::Format("pSigma0Graph_%i", iterID));
            FitResult_pSigma0.SetTitle(
                TString::Format("pSigma0Graph_%i", iterID));
            fitter->GetFitGraph(0, FitResult_pSigma0);

            double Chi2_pSigma0 = 0;
            double EffNumBins_pSigma0 = 0;
            int maxkStarBin = dataHist->FindBin(250);
            for (unsigned uBin = 1; uBin <= maxkStarBin; uBin++) {

              double mom = dataHist->GetBinCenter(uBin);
              //double dataX;
              double dataY;
              double dataErr;
              double theoryX;
              double theoryY;

              if (mom > femtoFitRegionUp[femtoFitIter]) {
                continue;
              }

              FitResult_pSigma0.GetPoint(uBin - 1, theoryX, theoryY);
              if (mom != theoryX) {
                std::cerr << "PROBLEM Sigma0 " << mom << '\t' << theoryX
                          << std::endl;
              }
              dataY = dataHist->GetBinContent(uBin);
              dataErr = dataHist->GetBinError(uBin);
              Chi2_pSigma0 += (dataY - theoryY) * (dataY - theoryY)
                  / (dataErr * dataErr);
              ++EffNumBins_pSigma0;
            }
            double pvalpSigma0 = TMath::Prob(Chi2_pSigma0,
                                             round(EffNumBins_pSigma0));
            double nSigmapSigma0 = TMath::Sqrt(2)
                * TMath::ErfcInverse(pvalpSigma0);

            if (iterID == 0) {
              std::cout << "=============\n";
              std::cout << "Fitter output\n";
              std::cout << "BL a  " << bl_a << " " << bl_a_err << "\n";
              std::cout << "BL b  " << bl_b << " " << bl_b_err << "\n";
              std::cout << "Cl    " << Cl << "\n";
              std::cout << "Chi2\n";
              std::cout << " glob " << chi2 << "\n";
              std::cout << " loc  " << Chi2_pSigma0 / round(EffNumBins_pSigma0)
                        << "\n";
              std::cout << "p-val\n";
              std::cout << " glob " << pval << "\n";
              std::cout << " loc  " << pvalpSigma0 << "\n";
              std::cout << "Neg?  " << isCFneg << "\n";
              std::cout << "=============\n";
            }

            /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            /// Write out all the stuff

            TGraph grCFSigmaRaw;
            grCFSigmaRaw.SetName(Form("Sigma0Raw_%i", iterID));
            TGraph grCFSigmaMain;
            grCFSigmaMain.SetName(Form("Sigma0Main_%i", iterID));
            TGraph grCFSigmaFeed;
            grCFSigmaFeed.SetName(Form("Sigma0Feed_%i", iterID));
            TGraph grCFSigmaSideband;
            grCFSigmaSideband.SetName(Form("Sigma0Sideband_%i", iterID));

            for (unsigned int i = 0; i < Ck_pSigma0->GetNbins(); ++i) {
              const float mom = Ck_pSigma0->GetBinCenter(0, i);
              const float baseline = bl_a + bl_b * mom;
              grCFSigmaRaw.SetPoint(i, mom, CkDec_pSigma0.EvalCk(mom) * baseline);
              grCFSigmaMain.SetPoint(
                  i,
                  mom,
                  (((CkDec_pSigma0.EvalMain(mom) - 1.)
                      * lambdaParams[lambdaIter].GetLambdaParam(
                          CATSLambdaParam::Primary)) + 1) * baseline);
              grCFSigmaFeed.SetPoint(i, mom, CkDec_pSigma0.EvalMainFeed(mom) * baseline);
              grCFSigmaSideband.SetPoint(
                  i,
                  mom,
                  (((Ck_SideBand->Eval(mom) - 1.)
                      * lambdaParams[lambdaIter].GetLambdaParam(
                          CATSLambdaParam::Primary, CATSLambdaParam::Fake, 0, 0))
                      + 1) * baseline);
            }

            param->cd();
            param->mkdir(TString::Format("Graph_%i", iterID));
            param->cd(TString::Format("Graph_%i", iterID));

            /// beautification
            DreamPlot::SetStyleHisto(dataHist, 24, kBlack);
            dataHist->SetTitle(";#it{k}* (MeV/#it{c}); C(#it{k}*)");
            DreamPlot::SetStyleHisto(SBmerge, 20, kRed + 2);
            SBmerge->SetTitle(";#it{k}* (MeV/#it{c}); C_{sideband}(#it{k}*)");
            DreamPlot::SetStyleHisto(sidebandHistLow, 26, kGreen + 2);
            sidebandHistLow->SetTitle(
                ";#it{k}* (MeV/#it{c}); C_{sideband, low}(#it{k}*)");
            DreamPlot::SetStyleHisto(sidebandHistUp, 26, kCyan + 2);
            sidebandHistUp->SetTitle(
                ";#it{k}* (MeV/#it{c}); C_{sideband, up}(#it{k}*)");
            sideband->SetLineWidth(2);
            sideband->SetLineStyle(2);
            sideband->SetLineColor(kGray + 1);
            FitResult_pSigma0.SetLineWidth(2);
            FitResult_pSigma0.SetLineColor(kRed + 2);
            grCFSigmaSideband.SetLineWidth(2);
            grCFSigmaSideband.SetLineStyle(2);
            grCFSigmaSideband.SetLineColor(kGray + 1);

            grPrefitContour->Write("fitContour");
            grCFSigmaRaw.Write();
            grCFSigmaMain.Write();
            grCFSigmaFeed.Write();
            grCFSigmaSideband.Write();
            sideband->Write(Form("SidebandFitNotScaled_%i", iterID));
            SBmerge->Write(Form("SidebandMerged_%i", iterID));
            FitResult_pSigma0.Write(Form("Fit_%i", iterID));

            if (fastPlot || iterID == 0) {
              dataHist->Write("CF");
              sidebandHistLow->Write("SidebandLow");
              sidebandHistUp->Write("SidebandUp");

              auto c = new TCanvas("DefaultFit", "DefaultFit");
              dataHist->GetXaxis()->SetRangeUser(0., 600);
              dataHist->GetYaxis()->SetRangeUser(0.8, 1.6);
              dataHist->Draw();
              FitResult_pSigma0.Draw("l3same");
              grCFSigmaSideband.Draw("l3same");

              auto info = new TPaveText(0.5, useBaseline ? 0.505 : 0.58, 0.88,
                                        0.85, "blNDC");
              info->SetBorderSize(0);
              info->SetTextSize(0.04);
              info->SetFillColor(kWhite);
              info->SetTextFont(42);
              TString SOURCE_NAME = "Gauss";
              double Yoffset = 1.2;
              info->AddText(
                  TString::Format(
                      "#it{r}_{%s} = %.3f #pm %.3f fm", SOURCE_NAME.Data(),
                      fitter->GetParameter("pSigma0", DLM_Fitter1::p_sor0),
                      fitter->GetParError("pSigma0", DLM_Fitter1::p_sor0)));
              info->AddText(
                  TString::Format("#it{a} = %.3f #pm %.3f", bl_a, bl_a_err));

              if (useBaseline) {
                info->AddText(
                    TString::Format("#it{b} = (%.3f #pm %.3f ) #times 10^{-4}",
                                    bl_b * 1e4, bl_b_err * 1e4));
              }
              info->AddText(
                  TString::Format("#chi_{loc}^{2}/ndf=%.1f/%.0f = %.3f",
                                  Chi2_pSigma0, EffNumBins_pSigma0,
                                  Chi2_pSigma0 / double(EffNumBins_pSigma0)));
              info->AddText(
                  TString::Format("#it{p}_{val}=%.3f, n_{#sigma}=%.3f",
                                  pvalpSigma0, nSigmapSigma0));

              info->Draw("same");
              c->Write("CFplot");
              if (!batchmode) {
                if (potential == 0) {
                  c->Print(
                      Form("%s/CF_pSigma0_%.3f_%.3f_%.3f.pdf", OutputDir.Data(),
                           d0, REf0inv, IMf0inv));
                } else if (potential == 1) {
                  c->Print(
                      Form("%s/CF_pSigma0_%.1f_%.4f_%.7f_%.2f_%.5f_%.8f.pdf",
                           OutputDir.Data(), deltap0, deltap1, deltap2, etap0,
                           etap1, etap2));
                } else {
                  c->Print(Form("%s/CF_pSigma0.pdf", OutputDir.Data()));
                }
              }

              auto d = new TCanvas("SidebandFit", "SidebandFit");
              SBmerge->GetXaxis()->SetRangeUser(0., 600);
              SBmerge->GetYaxis()->SetRangeUser(0.8, 1.6);
              SBmerge->Draw();
              sideband->Draw("l3same");
              d->Write("CFsideband");
              if (!batchmode) {
                d->Print(Form("%s/CF_pSideband.pdf", OutputDir.Data()));
              }

              auto e = new TCanvas("Sidebands", "Sidebands");
              SBmerge->Draw();
              sideband->Draw("l3same");
              sidebandHistLow->Draw("same");
              sidebandHistUp->Draw("same");
              auto leg = new TLegend(0.5, 0.6, 0.88, 0.85);
              leg->SetTextFont(42);
              leg->SetTextSize(0.05);
              leg->AddEntry(sidebandHistLow, "Sideband low", "pe");
              leg->AddEntry(sidebandHistUp, "Sideband up", "pe");
              leg->AddEntry(SBmerge, "Sideband merged", "pe");
              leg->AddEntry(sideband, "Fit", "l");
              leg->Draw("same");
              e->Write("CFsideband");
              if (!batchmode) {
                e->Print(Form("%s/CF_pSideband_all.pdf", OutputDir.Data()));
              }

              delete c;
              delete info;
              delete d;
              delete e;
            }

            param->cd();
            ntBuffer[0] = iterID;
            ntBuffer[1] = femtoFitRegionUp[femtoFitIter];
            ntBuffer[2] = (float) blIter;
            ntBuffer[3] = sourceSize[sizeIter];
            ntBuffer[4] = bl_a;
            ntBuffer[5] = bl_a_err;
            ntBuffer[6] = bl_b;
            ntBuffer[7] = bl_b_err;
            ntBuffer[8] = sideband->GetParameter(0);
            ntBuffer[9] = sideband->GetParError(0);
            ntBuffer[10] = sideband->GetParameter(1);
            ntBuffer[11] = sideband->GetParError(1);
            ntBuffer[12] = sideband->GetParameter(2);
            ntBuffer[13] = sideband->GetParError(2);
            ntBuffer[14] = sideband->GetParameter(3);
            ntBuffer[15] = sideband->GetParError(3);
            ntBuffer[16] = sideband->GetParameter(4);
            ntBuffer[17] = sideband->GetParError(4);
            ntBuffer[18] = sideband->GetParameter(5);
            ntBuffer[19] = sideband->GetParError(5);
            ntBuffer[20] = lambdaParams[lambdaIter].GetLambdaParam(
                CATSLambdaParam::Primary);
            ntBuffer[21] = lambdaParams[lambdaIter].GetLambdaParam(
                CATSLambdaParam::Primary, CATSLambdaParam::Fake, 0, 0);
            ntBuffer[22] = sidebandNormDown[sbNormIter];
            ntBuffer[23] = sidebandNormUp[sbNormIter];
            ntBuffer[24] = chi2;
            ntBuffer[25] = pval;
            ntBuffer[26] = Chi2_pSigma0;
            ntBuffer[27] = (float) EffNumBins_pSigma0;
            ntBuffer[28] = Chi2_pSigma0 / double(EffNumBins_pSigma0);
            ntBuffer[29] = pvalpSigma0;
            ntBuffer[30] = nSigmapSigma0;
            ntBuffer[31] = (float) isCFneg;
            if (potential == 0) {
              ntBuffer[32] = d0;
              ntBuffer[33] = REf0inv;
              ntBuffer[34] = IMf0inv;
            } else if (potential == 1) {
              ntBuffer[32] = deltap0;
              ntBuffer[33] = deltap1;
              ntBuffer[34] = deltap2;
              ntBuffer[35] = etap0;
              ntBuffer[36] = etap1;
              ntBuffer[37] = etap2;
            }

            ntResult->Fill(ntBuffer);
            ++iterID;

            delete fitter;

            if (NumIter == 0) {
              std::cout << "Skipping all systematic variations \n";
              goto exitThroughTheGiftShop;
            }
          }
        }
        delete sideband;
        delete Ck_SideBand;
      }
    }
  }
  exitThroughTheGiftShop:

  ntResult->Write();
  param->Close();

  delete side;
  delete CATSinput;
  delete ntResult;
  delete param;
  delete tidy;
  return;
}

/// =====================================================================================
void FitSigma0(char *argv[]) {
  const unsigned& NumIter = atoi(argv[1]);
  TString InputDir = argv[2];
  TString trigger = argv[3];
  TString suffix = argv[4];
  TString OutputDir = argv[5];
  const int potential = atoi(argv[6]);
  std::vector<double> params;
  if (potential == 0) {
    if (!argv[7] || !argv[8] || !argv[9]) {
      std::cout << "ERROR: Missing the scattering parameters\n";
      return;
    }
    params.push_back(atof(argv[7]));  // d0
    params.push_back(atof(argv[8]));  // REf0inv
    params.push_back(atof(argv[9]));  // IMf0inv
  } else if (potential == 1) {
    if (!argv[7] || !argv[8] || !argv[9] || !argv[10] || !argv[11]
        || !argv[12]) {
      std::cout << "ERROR: Missing the parameters for delta/eta\n";
      return;
    }
    params.push_back(atof(argv[7]));   // deltap0
    params.push_back(atof(argv[8]));   // deltap1
    params.push_back(atof(argv[9]));   // deltap2
    params.push_back(atof(argv[10]));  // etap0
    params.push_back(atof(argv[11]));  // etap1
    params.push_back(atof(argv[12]));  // etap2
  }
  FitSigma0(NumIter, InputDir, trigger, suffix, OutputDir, potential, params);
}
