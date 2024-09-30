#include "Fitter.h"

std::ostream &operator<<(std::ostream &os, const Fitter &fitter)
{
	os << std::setw(30) << std::endl;
	os << std::setw(30) << "Fitter runs with: " << std::endl;
	os << std::setw(30) << "_min_param_val: " << fitter._min_param_val << std::endl;
	os << std::setw(30) << "_max_param_val: " << fitter._max_param_val << std::endl;
	os << std::setw(30) << "_fit_norm_param_fix: " << fitter._fit_norm_param_fix << std::endl;
	os << std::setw(30) << "_fit_noise_param_set: " << fitter._fit_noise_param_set << std::endl;
	os << std::setw(30) << "_fit_noise_min_lim: " << fitter._fit_noise_min_lim << std::endl;
	os << std::setw(30) << "_fit_noise_max_lim: " << fitter._fit_noise_max_lim << std::endl;
	os << std::setw(30) << "_min_accepted_noise: " << fitter._min_accepted_noise << std::endl;
	os << std::setw(30) << "_max_accepted_noise: " << fitter._max_accepted_noise << std::endl;
	os << std::setw(30) << "_max_refit: " << fitter._max_refit << std::endl;
	os << std::setw(30) << "_fit_tolerance: " << fitter._fit_tolerance << std::endl;
	os << std::setw(30) << "_fit_max_it: " << fitter._fit_max_it << std::endl;
	os << std::setw(30) << "_fit_fun_cal: " << fitter._fit_fun_cal << std::endl;

	return os;
}

/*
 * Initializes class members taking the values from the file
 */

void Fitter::init(const char *config_file)
{

	if (config.ReadFile(config_file, kEnvUser) < 0)
	{
		MSG(ERR, "[FIT] Config file not loaded --> exit.");
		exit(0);
	}
	else
	{
		_min_param_val = config.GetValue("_min_param_val", 0);
		_max_param_val = config.GetValue("_max_param_val", 0);
		_fit_norm_param_fix = config.GetValue("_fit_norm_param_fix", 0);
		_fit_noise_param_set = config.GetValue("_fit_noise_param_set", 0.);
		_fit_noise_min_lim = config.GetValue("_fit_noise_min_lim", 0.1);
		_fit_noise_max_lim = config.GetValue("_fit_noise_max_lim", 10);
		_min_accepted_noise = config.GetValue("_min_accepted_noise", 0.1);
		_max_accepted_noise = config.GetValue("_max_accepted_noise", 10);
		_max_refit = config.GetValue("_max_refit", 1);
		_fit_tolerance = config.GetValue("_fit_tolerance", 1e-3);
		_fit_max_it = config.GetValue("_fit_max_it", 1000);
		_fit_fun_cal = config.GetValue("_fit_fun_cal", 1000);
	}
}

/*
 * Return S_curve
 */
TGraph *Fitter::get_S_curve()
{
	return s_curve;
}

/*
 * Return S_curce fit parameters
 */
std::pair<double, double> Fitter::get_S_curve_fit_params()
{
	return fit_params;
}

/*
 * Return S_curce fit chi2
 */
double Fitter::get_S_curve_fit_chi2()
{
	// Not well written
	return ferf->GetChisquare() / (double)(ferf->GetNDF());
}

/*
 * Find vector min max value
 */
void Fitter::find_min_max(std::vector<int> &v, int &min, int &max)
{
	min = *std::min_element(v.begin(), v.end());
	max = *std::max_element(v.begin(), v.end());
}
/*
 * Sets general fitting parameters
 */
void Fitter::set_fitting_options()
{
	ROOT::Math::MinimizerOptions::SetDefaultTolerance(_fit_tolerance);
	ROOT::Math::MinimizerOptions::SetDefaultMaxIterations(_fit_max_it);
	ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(_fit_fun_cal);
}

/*
 * Makes fit to the global variable ferf, s_curves
 */
void Fitter::make_fit()
{
	s_curve->Fit(ferf, "WQ");
	fit_params = std::make_pair(ferf->GetParameter(1), ferf->GetParameter(2));
	;
}

/*
 * Check if fit is good
 */
bool Fitter::is_good_fit()
{

	bool bad_noise = (fit_params.second < _min_accepted_noise) || (fit_params.second > _max_accepted_noise);
	bool bad_mean = (fit_params.first < _min_param_val) || (fit_params.first > _max_param_val);

	return !(bad_noise || bad_mean);
}

/*
 * Fit the S-curves
 */
int Fitter::fit_error_function(std::vector<int> &v_x, std::vector<int> &v_y)
{
	const int param_nb = v_x.size();
	int fit_failed{0};

	// Set preliminary parameters
	set_fitting_options();
	find_min_max(v_x, _min_param_val, _max_param_val);

	// MSG(DEB, "[FIT] Min param found: " + std::to_string(_min_param_val) + " Max param: " + std::to_string(_max_param_val)  );

	// Define fit
	s_curve = new TGraph(param_nb, &v_x[0], &v_y[0]);
	s_curve->SetLineWidth(2);
	s_curve->SetLineColor(kBlue);
	ferf = new TF1("ferf", "0.5*[0]*(1+TMath::Erf((x-[1])/(TMath::Sqrt2()*[2])))", 0, 150);

	// Set preliminary parameters
	ferf->FixParameter(0, _fit_norm_param_fix);
	ferf->SetParLimits(1, _min_param_val, _max_param_val);
	ferf->SetParLimits(2, _fit_noise_min_lim, _fit_noise_max_lim);
	ferf->SetParameters(_fit_norm_param_fix, _min_param_val, _fit_noise_param_set);
	ferf->SetParError(1, 0.1);
	ferf->SetParError(2, 0.01);

	make_fit();

	if (is_good_fit() == false)
	{
		for (int i = 1; i <= _max_refit; i++)
		{
			ferf->FixParameter(0, _fit_norm_param_fix);
			ferf->SetParLimits(2, 0.5, _fit_noise_max_lim);
			ferf->SetParameters(_fit_norm_param_fix, ferf->GetParameter(1), _fit_noise_max_lim);
			// ferf	->	SetParLimits(1, 100,_max_param_val );
			// ferf 	->	SetParLimits(2, 0.1,_fit_noise_max_lim);

			make_fit();
			_refitted++;
			if (is_good_fit())
				break;
		}
	}
	// Check if finally fit was good or not

	if (is_good_fit())
		return 1;
	else
		return 0;
}

/*
 * Clear global variables for the next fit
 */
void Fitter::clear()
{
	fit_params = std::make_pair(0.0, 0.0);
	s_curve = nullptr;
	ferf = nullptr;
}
