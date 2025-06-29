/** @file thermodynamics.c Documented thermodynamics module
 *
 * Julien Lesgourgues, 6.09.2010
 * Manuel Buen-Abad, 2019-2020: include thermodynamics stiff DE solver, and routines for Interacting Dark Matter-baryon scattering.
 *
 * Deals with the thermodynamical evolution.
 * This module has two purposes:
 *
 * - at the beginning, to initialize the thermodynamics, i.e. to
 *   integrate the thermodynamical equations, and store all
 *   thermodynamical quantities as a function of redshift inside an
 *   interpolation table. The current version of recombination is
 *   based on RECFAST v1.5. The current version of reionization is
 *   based on exactly the same reionization function as in CAMB, in
 *   order to make allow for comparison. It should be easy to
 *   generalize the module to more complicated reionization histories.
 *
 * - to provide a routine which allow other modules to evaluate any
 *   thermodynamical quantities at a given redshift value (by
 *   interpolating within the interpolation table).
 *
 *
 * The logic is the following:
 *
 * - in a first step, the code assumes that there is no reionization,
 *   and computes the ionization fraction, Thomson scattering rate,
 *   baryon temperature, etc., using RECFAST. The result is stored in
 *   a temporary table 'recombination_table' (within a temporary
 *   structure of type 'recombination') for each redshift in a range 0
 *   < z < z_initial.  The sampling in z space is done with a simple
 *   linear step size.
 * - in a second step, the code adds the reionization history,
 *   starting from a redshift z_reio_start. The ionization fraction at
 *   this redshift is read in the previous recombination table in
 *   order to ensure a perfect matching. The code computes the
 *   ionization fraction, Thomson scattering rate, baryon temperature,
 *   etc., using a given parametrization of the reionization
 *   history. The result is stored in a temporary table
 *   'reionization_table' (within a temporary structure of type
 *   'reionization') for each redshift in the range 0 < z <
 *   z_reio_start. The sampling in z space is found automatically,
 *   given the precision parameter 'reionization_sampling'.
 *
 * - in a third step, the code merges the two tables
 *   'recombination_table' and 'reionization_table' inside the table
 *   'thermodynamics_table', and the temporary structures
 *   'recombination' and 'reionization' are freed. In
 *   'thermodynamics_table', the sampling in z space is the one
 *   defined in the recombination algorithm for z_reio_start < z <
 *   z_initial, and the one defined in the reionization algorithm for
 *   0 < z < z_reio_start.
 *
 * - at this stage, only a few columns in the table
 *   'thermodynamics_table' have been filled. In a fourth step, the
 *   remaining columns are filled, using some numerical
 *   integration/derivation routines from the 'array.c' tools module.
 *
 * - small detail: one of the columns contains the maximum variation
 *   rate of a few relevant thermodynamical quantities. This rate
 *   will be used for defining automatically the sampling step size in
 *   the perturbation module. Hence, the exact value of this rate is
 *   unimportant, but its order of magnitude at a given z defines the
 *   sampling precision of the perturbation module. Hence, it is
 *   harmless to use a smoothing routine in order to make this rate
 *   look nicer, although this will not affect the final result
 *   significantly. The last step in the thermodynamics_init module is
 *   to perform this smoothing.
 *
 * In summary, the following functions can be called from other modules:
 *
 * -# thermodynamics_init() at the beginning (but after background_init())
 * -# thermodynamics_at_z() at any later time
 * -# thermodynamics_free() at the end, when no more calls to thermodynamics_at_z() are needed
 */

#include "thermodynamics.h"

#ifdef HYREC
#include "hyrec.h"
#endif

/**
 * Thermodynamics quantities at given redshift z.
 *
 * Evaluates all thermodynamics quantities at a given value of
 * the redshift by reading the pre-computed table and interpolating.
 *
 * @param pba        Input: pointer to background structure
 * @param pth        Input: pointer to the thermodynamics structure (containing pre-computed table)
 * @param z          Input: redshift
 * @param inter_mode Input: interpolation mode (normal or growing_closeby)
 * @param last_index Input/Output: index of the previous/current point in the interpolation array (input only for closeby mode, output for both)
 * @param pvecback   Input: vector of background quantities (used only in case z>z_initial for getting ddkappa and dddkappa; in that case, should be already allocated and filled, with format short_info or larger; in other cases, will be ignored)
 * @param pvecthermo Output: vector of thermodynamics quantities (assumed to be already allocated)
 * @return the error status
 */

int thermodynamics_at_z(
                        struct background * pba,
                        struct thermo * pth,
                        double z,
                        short inter_mode,
                        int * last_index,
                        double * pvecback,
                        double * pvecthermo
                        ) {

  /** Summary: */

  /** - define local variables */

  double x0;
//MANUEL
  double zdec,Tx,dTxdz;
//MANUEL

  /* - the fact that z is in the pre-computed range 0 <= z <= z_initial
     will be checked in the interpolation routines below. Before
     trying to interpolate, allow the routine to deal with the case z
     > z_intial: then, all relevant quantities can be extrapolated
     using simple analytic approximations */

  if (z >= pth->z_table[pth->tt_size-1]) {

    /* ionization fraction assumed to remain constant at large z */
    x0= pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_xe];
    pvecthermo[pth->index_th_xe] = x0;

    /* Calculate dkappa/dtau (dkappa/dtau = a n_e x_e sigma_T = a^{-2} n_e(today) x_e sigma_T in units of 1/Mpc) */
    pvecthermo[pth->index_th_dkappa] = (1.+z) * (1.+z) * pth->n_e * x0 * _sigma_ * _Mpc_over_m_;

    /* tau_d scales like (1+z)**2 */
    pvecthermo[pth->index_th_tau_d] = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_d]*pow((1+z)/(1.+pth->z_table[pth->tt_size-1]),2);

    if (pth->compute_damping_scale == _TRUE_) {

      /* r_d scales like (1+z)**-3/2 */
      pvecthermo[pth->index_th_r_d] = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_r_d]*pow((1+z)/(1.+pth->z_table[pth->tt_size-1]),-1.5);

    }

    /* Calculate d2kappa/dtau2 = dz/dtau d/dz[dkappa/dtau] given that [dkappa/dtau] proportional to (1+z)^2 and dz/dtau = -H */
    pvecthermo[pth->index_th_ddkappa] = -pvecback[pba->index_bg_H] * 2. / (1.+z) * pvecthermo[pth->index_th_dkappa];

    /* Calculate d3kappa/dtau3 given that [dkappa/dtau] proportional to (1+z)^2 */
    pvecthermo[pth->index_th_dddkappa] = (pvecback[pba->index_bg_H]*pvecback[pba->index_bg_H]/ (1.+z) - pvecback[pba->index_bg_H_prime]) * 2. / (1.+z) * pvecthermo[pth->index_th_dkappa];

    /* \f$ exp^{-\kappa}, g, g', g'' \f$ can be set to zero: they are
       used only for computing the source functions in the
       perturbation module; but source functions only need to be
       sampled below z_initial (the condition that
       z_start_sources<z_initial is checked in the perturbation
       module) */
    pvecthermo[pth->index_th_exp_m_kappa] = 0.;
    pvecthermo[pth->index_th_g]=0.;
    pvecthermo[pth->index_th_dg]=0.;
    pvecthermo[pth->index_th_ddg]=0.;

    /* Calculate Tb */
    pvecthermo[pth->index_th_Tb] = pba->T_cmb*(1.+z);

    /* Calculate baryon equation of state parameter wb = (k_B/mu) Tb */
    /* note that m_H / mu = 1 + (m_H/m_He-1) Y_p + x_e (1-Y_p) */
    pvecthermo[pth->index_th_wb] = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * pth->YHe + x0 * (1.-pth->YHe)) * pba->T_cmb * (1.+z);

    /* Calculate baryon adiabatic sound speed cb2 = (k_B/mu) Tb (1-1/3 dlnTb/dlna) = (k_B/mu) Tb (1+1/3 (1+z) dlnTb/dz) */
    /* note that m_H / mu = 1 + (m_H/m_He-1) Y_p + x_e (1-Y_p) */
    pvecthermo[pth->index_th_cb2] = pvecthermo[pth->index_th_wb] * 4. / 3.;

    /* derivatives of baryon sound speed (only computed if some non-minimal tight-coupling schemes is requested) */
    if (pth->compute_cb2_derivatives == _TRUE_) {

      /* since cb2 proportional to (1+z) or 1/a, its derivative wrt conformal time is given by dcb2 = - a H cb2 */
      pvecthermo[pth->index_th_dcb2] = - pvecback[pba->index_bg_H] * pvecback[pba->index_bg_a] * pvecthermo[pth->index_th_cb2];

      /* then its second derivative is given by ddcb2 = - a H' cb2 */
      pvecthermo[pth->index_th_ddcb2] = - pvecback[pba->index_bg_H_prime] * pvecback[pba->index_bg_a] * pvecthermo[pth->index_th_cb2];
    }

    /* in this regime, variation rate = dkappa/dtau */
    pvecthermo[pth->index_th_rate] = pvecthermo[pth->index_th_dkappa];

//MANUEL

    if (pth->compute_dm_thermo == _TRUE_) {

      // defining the chiDM (IDM-b) temperature and its derivative w.r.t. redshift, for various cases:
      if (pth->realistic_Tx == _TRUE_) {
        if ((pth->sigma_0 > 0.) && (pth->pow_n > -3.)) {

          zdec = pth->appx_z_thdec;//approximate redshift of thermal decoupling; we use this one to match case from dmdecoupling routine
          // zdec = pth->z_thdec;//exact redshift of thermal decoupling
          zdec = MIN(zdec, pth->z_nr);

          if (z >= zdec) {
            Tx = pba->T_cmb*(1.+z);
            dTxdz = pba->T_cmb;
          }
          else {
            Tx = pba->T_cmb*(1.+zdec)*pow((1.+z)/(1.+zdec),2.);
            dTxdz = 2*Tx/(1.+z);
          }
        }
        else if (pth->pow_n <= -3.) {
          Tx = pba->T_cmb*(1.+pth->z_fo)*pow((1.+z)/(1.+pth->z_fo),2.);
          dTxdz = 2*Tx/(1.+z);
        }
      }
      else {
        if ((pth->sigma_0 > 0.) && (pth->pow_n > -3.)) {

          zdec = pth->appx_z_thdec;//approximate redshift of thermal decoupling; we use this one to match case from dmdecoupling routine
          // zdec = pth->z_thdec;//exact redshift of thermal decoupling

          if (z >= zdec) {
            Tx = pba->T_cmb*(1.+z);
            dTxdz = pba->T_cmb;
          }
          else {
            Tx = pba->T_cmb*(1.+zdec)*pow((1.+z)/(1.+zdec),2.);
            dTxdz = 2*Tx/(1.+z);
          }
        }
        else if (pth->pow_n <= -3.) {
          Tx = 0.;
          dTxdz = 0.;
        }
      }

      pvecthermo[pth->index_th_Tx] = Tx;
      pvecthermo[pth->index_th_cx2] = (_k_B_ / ( _c_ * _c_ * pth->mx * _MeV_over_kg_ )) * (Tx + pth->pref_cx2*(1.+z)*dTxdz/3.);
      pvecthermo[pth->index_th_Rx] = (1.+z)*(1.+z) * pth->pref1 * pow(pba->Vrms2/3. + pth->pref2*(pba->T_cmb*(1.+z) + pth->pref3*Tx), (pth->pow_n + 1.)/2.);
      pvecthermo[pth->index_th_Rxprime] = pth->mxb * pvecthermo[pth->index_th_Rx];
      pvecthermo[pth->index_th_dRx] = -pvecback[pba->index_bg_H]*(2./(1+z) + ((pth->pow_n + 1.)/2.)*( pth->pref2*(pba->T_cmb + pth->pref3*dTxdz) )/( pba->Vrms2/3. + pth->pref2*(pba->T_cmb*(1.+z) + pth->pref3*Tx) ) )*pvecthermo[pth->index_th_Rx];
     
    }
//MANUEL

    /* quantities related to DM interacting with DR */
    if(pba->has_idm_dr == _TRUE_){

      /* calculate dmu_idm_dr and approximate its derivatives as zero */
      pvecthermo[pth->index_th_dmu_idm_dr] = pth->a_idm_dr*pow((1.+z)/1.e7,pth->nindex_idm_dr)*pba->Omega0_idm_dr*pow(pba->h,2);
      pvecthermo[pth->index_th_ddmu_idm_dr] =  -pvecback[pba->index_bg_H] * pth->nindex_idm_dr / (1+z) * pvecthermo[pth->index_th_dmu_idm_dr];
      pvecthermo[pth->index_th_dddmu_idm_dr] = (pvecback[pba->index_bg_H]*pvecback[pba->index_bg_H]/ (1.+z) - pvecback[pba->index_bg_H_prime])
        *  pth->nindex_idm_dr / (1.+z) * pvecthermo[pth->index_th_dmu_idm_dr];

      /* calculate dmu_idr (self interaction) */
      pvecthermo[pth->index_th_dmu_idr] = pth->b_idr*pow((1.+z)/1.e7,pth->nindex_idm_dr)*pba->Omega0_idr*pow(pba->h,2);

      /* extrapolate optical depth of idm_dr and idr */
      pvecthermo[pth->index_th_tau_idm_dr] = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idm_dr]+
        (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idm_dr]-pth->thermodynamics_table[(pth->tt_size-2)*pth->th_size+pth->index_th_tau_idm_dr])
        *(z-pth->z_table[pth->tt_size-1])/(pth->z_table[pth->tt_size-1]-pth->z_table[pth->tt_size-2]);

      pvecthermo[pth->index_th_tau_idr] = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idr]+
        (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idr]-pth->thermodynamics_table[(pth->tt_size-2)*pth->th_size+pth->index_th_tau_idr])
        *(z-pth->z_table[pth->tt_size-1])/(pth->z_table[pth->tt_size-1]-pth->z_table[pth->tt_size-2]);

      /* extrapolate idm_dr visibility function */
      pvecthermo[pth->index_th_g_idm_dr] = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_g_idm_dr];

      /* calculate interacting dark matter sound speed */
      pvecthermo[pth->index_th_cidm_dr2] = 4*_k_B_*pba->T_idr*(1.+z)/_eV_/3./pth->m_idm;

      /* calculate interacting dark matter temperature (equal to idr temperature at this redhsift) */
      pvecthermo[pth->index_th_Tidm_dr] = pba->T_idr*(1.+z);
    }

  }

  /** - interpolate in table with array_interpolate_spline() (normal
      mode) or array_interpolate_spline_growing_closeby() (closeby
      mode) */

  else {

    /* some very specific cases require linear interpolation because of a break in the derivative of the functions */
    if (((pth->reio_parametrization == reio_half_tanh) && (z < 2*pth->z_reio))
        || ((pth->reio_parametrization == reio_inter) && (z < 50.))) {

      class_call(array_interpolate_linear(
                                          pth->z_table,
                                          pth->tt_size,
                                          pth->thermodynamics_table,
                                          pth->th_size,
                                          z,
                                          last_index,
                                          pvecthermo,
                                          pth->th_size,
                                          pth->error_message),
                 pth->error_message,
                 pth->error_message);
    }

    /* in the "normal" case, use spline interpolation */
    else {

      if (inter_mode == pth->inter_normal) {

        class_call(array_interpolate_spline(
                                            pth->z_table,
                                            pth->tt_size,
                                            pth->thermodynamics_table,
                                            pth->d2thermodynamics_dz2_table,
                                            pth->th_size,
                                            z,
                                            last_index,
                                            pvecthermo,
                                            pth->th_size,
                                            pth->error_message),
                   pth->error_message,
                   pth->error_message);
      }

      if (inter_mode == pth->inter_closeby) {

        class_call(array_interpolate_spline_growing_closeby(
                                                            pth->z_table,
                                                            pth->tt_size,
                                                            pth->thermodynamics_table,
                                                            pth->d2thermodynamics_dz2_table,
                                                            pth->th_size,
                                                            z,
                                                            last_index,
                                                            pvecthermo,
                                                            pth->th_size,
                                                            pth->error_message),
                   pth->error_message,
                   pth->error_message);

      }
    }
  }
  return _SUCCESS_;
}

/**
 * Initialize the thermo structure, and in particular the
 * thermodynamics interpolation table.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param pth Input/Output: pointer to initialized thermo structure
 * @return the error status
 */
int thermodynamics_init(
                        struct precision * ppr,
                        struct background * pba,
                        struct thermo * pth
                        ) {

  /** Summary: */

  /** - define local variables */

  /* index running over time*/
  int index_tau;
  /* temporary variables related to visibility function */
  double g;
  /* vector of background values for calling background_at_tau() */
  double * pvecback;
  /* index for calling background_at_tau() */
  int last_index_back;
  /* temporary table of values of tau associated with z values in pth->z_table */
  double * tau_table;
  /* same ordered in growing time rather than growing redshift */
  double * tau_table_growing;
  /* conformal time of reionization */
  double tau_reio;
  /* R = (3./4.)*(rho_b/rho_g) */
  double R;

  /* structures for storing temporarily information on recombination
     and reionization */
  struct recombination reco;
  struct reionization reio;
  struct recombination * preco;
  struct reionization * preio;
//MANUEL
  struct dmdecoupling dmde;
  struct dmdecoupling * pdmde;
  struct thermo_vector thv;
  struct thermo_vector * pthv;
//MANUEL

  double tau,tau_ini;
  double g_max;
  int index_tau_max;
  double dkappa_ini;

  double z_idm_dr, z_idr, tau_idm_dr, tau_idr, Gamma_heat_idm_dr, dTdz_idm_dr, T_idm_dr, z, T_idr, dz, T_adia, z_adia;

  double tau_idm_dr_fs=0.;
  int index_tau_fs;
  int n, N_sub_steps;
  double dz_sub_step;

//MANUEL
  double mx_min,z_nr,uB,YB,rhob,xsec,p1,OmegaR,numer,denom,mu_over_T;
  int my_index;
  double my_tau,aH,knr,kthdec,ktcdec;
  short starts_coupled;
//MANUEL

  if (pth->thermodynamics_verbose > 0)
    printf("Computing thermodynamics");

  /** - compute and check primordial Helium fraction  */

  /* Y_He */
  if (pth->YHe == _BBN_) {
    class_call(thermodynamics_helium_from_bbn(ppr,pba,pth),
               pth->error_message,
               pth->error_message);
    if (pth->thermodynamics_verbose > 0)
      printf(" with Y_He=%.4f\n",pth->YHe);
  }
  else {
    if (pth->thermodynamics_verbose > 0)
      printf("\n");
  }

  class_test((pth->YHe < _YHE_SMALL_)||(pth->YHe > _YHE_BIG_),
             pth->error_message,
             "Y_He=%g out of bounds (%g<Y_He<%g)",pth->YHe,_YHE_SMALL_,_YHE_BIG_);

  /** - check energy injection parameters */

  class_test((pth->annihilation<0),
             pth->error_message,
             "annihilation parameter cannot be negative");

  class_test((pth->annihilation>1.e-4),
             pth->error_message,
             "annihilation parameter suspiciously large (%e, while typical bounds are in the range of 1e-7 to 1e-6)",
             pth->annihilation);

  class_test((pth->annihilation_variation>0),
             pth->error_message,
             "annihilation variation parameter must be negative (decreasing annihilation rate)");

  class_test((pth->annihilation_z<0),
             pth->error_message,
             "characteristic annihilation redshift cannot be negative");

  class_test((pth->annihilation_zmin<0),
             pth->error_message,
             "characteristic annihilation redshift cannot be negative");

  class_test((pth->annihilation_zmax<0),
             pth->error_message,
             "characteristic annihilation redshift cannot be negative");

  class_test((pth->annihilation>0) && ((pba->has_cdm==_FALSE_)&&(pba->has_idm_dr==_FALSE_)),
             pth->error_message,
             "CDM annihilation effects require the presence of CDM or IDM!");

  class_test((pth->annihilation_f_halo>0) && (pth->recombination==recfast),
             pth->error_message,
             "Switching on DM annihilation in halos requires using HyRec instead of RECFAST. Otherwise some values go beyond their range of validity in the RECFAST fits, and the thermodynamics module fails. Two solutions: add 'recombination = HyRec' to your input, or set 'annihilation_f_halo = 0.' (default).");

  class_test((pth->annihilation_f_halo<0),
             pth->error_message,
             "Parameter for DM annihilation in halos cannot be negative");

  class_test((pth->annihilation_z_halo<0),
             pth->error_message,
             "Parameter for DM annihilation in halos cannot be negative");

  if (pth->thermodynamics_verbose > 0)
    if ((pth->annihilation >0) && (pth->reio_parametrization == reio_none) && (ppr->recfast_Heswitch >= 3) && (pth->recombination==recfast))
      printf("Warning: if you have DM annihilation and you use recfast with option recfast_Heswitch >= 3, then the expression for CfHe_t and dy[1] becomes undefined at late times, producing nan's. This is however masked by reionization if you are not in reio_none mode.");

  class_test((pth->decay<0),
             pth->error_message,
             "decay parameter cannot be negative");

  class_test((pth->decay>0)&&((pba->has_cdm==_FALSE_)&&(pba->has_idm_dr==_FALSE_)),
             pth->error_message,
             "CDM decay effects require the presence of CDM or IDM!");

  /* tests in order to prevent segmentation fault in the following */
  class_test(_not4_ == 0.,
             pth->error_message,
             "stop to avoid division by zero");
  class_test(pth->YHe == 1.,
             pth->error_message,
             "stop to avoid division by zero");

//MANUEL
  //forbidding non-physical values for cross sections in chiDM-baryon interactions, and other tests
  if (pth->compute_dm_thermo == _TRUE_) {

    class_test((pba->has_chi == _FALSE_),
               pth->error_message,
               "This should never happen");

    class_test((pth->sigma_0 == 0.),
               pth->error_message,
               "This should never happen");

    class_test((pth->sigma_0 < 0),
               pth->error_message,
               "\tThe chiDM (IDM-b) scattering cross section, sigma_0, cannot be negative.\n");

    class_test((pth->mx <= 0.),
               pth->error_message,
               "\tThe chiDM (IDM-b) mass, mx, must be positive.\n");

    class_test((pth->sigmav_fo < 0),
               pth->error_message,
               "\tThe freeze-out cross section, sigmav_fo, cannot be negative.\n");

    class_test((pth->sigmav_fo == 0.),
               pth->error_message,
               "\tsigmav_fo = 0. Stop to avoid division by zero.\n");

    mx_min = 3*(pba->T_cmb*_k_B_/_eV_)*(1.+ppr->recfast_z_initial*ppr->dmde_buffer)/1.e6;
    z_nr = -1. + 1./3./( (_k_B_*pba->T_cmb)/(pth->mx * _MeV_over_kg_ * _c_ * _c_) );

    class_test(((pth->realistic_Tx == _TRUE_) && (pth->mx < mx_min)),
               pth->error_message,
               "\tIf the DM was ever in thermal equilibrium with the baryons, a mass mx=%.3e eV <= mx_min = %.3e eV means that the DM is relativistic (Tx/mx >= 1/3) at a redshift of z=%.4e (sufficiently before recombination for the purposes of CLASS), when the photon temperature is T_g=%.4e eV. In fact, it only became non-relativistic at z_nr=%.4e Therefore the chiDM (IDM-b) must be at some point treated as HDM/WDM by CLASS, which we did not foresee when we modified it. This means we need to stop CLASS. To fix, make the chiDM (IDM-b) mass bigger.\n",pth->mx*1.e6,mx_min*1.e6,ppr->recfast_z_initial*ppr->dmde_buffer,pba->T_cmb*_k_B_*(1.+ppr->recfast_z_initial*ppr->dmde_buffer)/_eV_,z_nr);
  }

  // now defining some useful prefactors and redshifts for the IDM-b model
  if (pth->compute_dm_thermo == _TRUE_) {

    uB =(pth->mB/_m_H_);

    if (pth->ignore_He == _TRUE_)
      YB = uB*(1.-pth->YHe);// counting only the baryons not associated with helium (in case of protons, those in He nuclei; in case of electrons, those that will recombine with it)
    else
      YB = uB*(1.-pth->YHe)*( 1. + 2.*pth->YHe/(_not4_*(1.-pth->YHe)) );// including the baryons associated with helium (in case of protons, those in He nuclei; in case of electrons, those that will recombine with it)

    // useful prefactors
    rhob = pba->Omega0_b * 3.*pow( _mpl_MeV_ * ( pba->H0/(_Mpc_over_m_*_MeV_times_m_) ), 2);// rho_b [MeV^4]
    xsec = pth->c_n*pth->sigma_0 * _MeV_times_m_ * _MeV_times_m_;// c_n*sigma_0 [MeV^-2]
    p1 = (YB*rhob*xsec)/(pth->mx + pth->mB/_MeV_over_kg_);// [MeV]

    pth->pref1 = p1 * _MeV_times_m_ * _Mpc_over_m_;// [1/Mpc]
    pth->pref2 = (_k_B_ / ( _c_ * _c_ * pth->mB ));// [1/K]
    pth->pref3 = (pth->mB / ( pth->mx * _MeV_over_kg_ ));// dimensionless

    if (pth->thermodynamics_verbose > 5)
      printf("Some useful quantities: rhob = %.4e MeV^4, xsec = %.4e MeV^-2\npref1 = %.4e MeV = %.4e 1/Mpc\n", rhob, xsec, p1, pth->pref1);

    // non-relativistic redshift
    pth->z_nr = -1. + 1./3./(pth->pref2*pth->pref3*pba->T_cmb);

    // energy density in radiation:
    OmegaR = pba->Omega0_g*(1.+7./8.*3.046*pow(4./11.,4./3.));

    if ((pth->sigma_0 > 0.) && (pth->pow_n > -3.) && (pth->mx > 0.)) {//case with n>-3

      numer = pba->H0*sqrt(OmegaR);
      denom = pth->pref1*pth->mxb;
      mu_over_T = (pth->muxb * _c_ * _c_)/(_k_B_*pba->T_cmb);

      // redshift at which IDM & b are still in thermal equilibrium, with a given ratio Rx/H > 1:
      pth->z_theq_ratio = -1. + pow(ppr->Rx_H_ratio*(numer/denom)*pow(mu_over_T, (pth->pow_n + 1.)/2.), 2./(3.+pth->pow_n));//assuming Radiation Domination
      // thermal decoupling redshift:
      pth->appx_z_thdec = -1. + pow((numer/denom)*pow(mu_over_T, (pth->pow_n + 1.)/2.), 2./(3.+pth->pow_n));//assuming Radiation Domination
    }
    if ((pth->sigmav_fo > 0.) && (pth->pow_n <= -3.) && (pth->mx > 0.)) {//case with n<=-3

      // all other redshift do not make sense, so instead we focus on:
      // freeze-out redshift:
      pth->z_fo = -1. + (sqrt(OmegaR)/pba->Omega0_chi)*(1./(pba->H0 * _c_ / _Mpc_over_m_))*(8./3.)*(_PI_*_G_*pth->mx*_MeV_over_kg_)*(1./pth->sigmav_fo);
    }
  }
//MANUEL

  /** - initialize pointers */

  preco=&reco;
  preio=&reio;
//MANUEL
  pdmde=&dmde;
  pthv=&thv;
//MANUEL

  /** - assign values to all indices in the structures with thermodynamics_indices()*/

  class_call(thermodynamics_indices(pba,pth,preco,preio,pdmde),//MANUEL: NOTE: added the dmdecoupling structure pdmde
             pth->error_message,
             pth->error_message);

  /** - allocate background vector */

  class_alloc(pvecback,pba->bg_size*sizeof(double),pba->error_message);

//MANUEL
  //solve the chiDM-baryon decoupling
  if (pth->compute_dm_thermo == _TRUE_) {

    class_call_except(thermodynamics_dmdecoupling(ppr,pba,pth,pdmde,pthv,pvecback),
               pth->error_message,
               pth->error_message,
               free(pdmde->tau_vec);//MANUEL: added this line
               free(pvecback));
  }
//MANUEL

  /** - solve recombination and store values of \f$ z, x_e, d \kappa / d \tau, T_b, c_b^2 \f$ with thermodynamics_recombination() */

  class_call_except(thermodynamics_recombination(ppr,pba,pth,preco,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
                    pth->error_message,
                    pth->error_message,
                    free(pdmde->dmdecoupling_table);//MANUEL: added this line
                    free(pdmde->z_vec);//MANUEL: added this line
                    free(pdmde->tau_vec);//MANUEL: added this line
                    free(pvecback));

  /** - if there is reionization, solve reionization and store values of \f$ z, x_e, d \kappa / d \tau, T_b, c_b^2 \f$ with thermodynamics_reionization()*/

  if (pth->reio_parametrization != reio_none) {
    class_call_except(thermodynamics_reionization(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
                      pth->error_message,
                      pth->error_message,
                      free(pdmde->dmdecoupling_table);//MANUEL: added this line
                      free(pdmde->z_vec);//MANUEL: added this line
                      free(pdmde->tau_vec);//MANUEL: added this line
                      free(preco->recombination_table);
                      free(pvecback));
  }
  else {
    preio->rt_size=0;
    preio->index_reco_when_reio_start=-1;
  }

  /** - merge tables in recombination and reionization structures into
      a single table in thermo structure */

  class_call(thermodynamics_merge_reco_and_reio(ppr,pba,pth,preco,preio,pdmde),//MANUEL: added pdmde
             pth->error_message,
             pth->error_message);

  /** - compute table of corresponding conformal times */

  class_alloc(tau_table,pth->tt_size*sizeof(double),pth->error_message);

  for (index_tau=0; index_tau < pth->tt_size; index_tau++) {
    class_call(background_tau_of_z(pba,
                                   pth->z_table[index_tau],
                                   tau_table+index_tau),
               pba->error_message,
               pth->error_message);
  }

  /** - store initial value of conformal time in the structure */

  pth->tau_ini = tau_table[pth->tt_size-1];

  /** - fill missing columns (quantities not computed previously but related) */

  /** - --> minus the baryon drag interaction rate, -dkappa_d/dtau = -[1/R * kappa'], with R = 3 rho_b / 4 rho_gamma, stored temporarily in column ddkappa */

  last_index_back = pba->bg_size-1;

  for (index_tau=0; index_tau < pth->tt_size; index_tau++) {

    class_call(background_at_tau(pba,
                                 tau_table[index_tau],
                                 pba->normal_info,
                                 pba->inter_closeby,
                                 &last_index_back,
                                 pvecback),
               pba->error_message,
               pth->error_message);

    R = 3./4.*pvecback[pba->index_bg_rho_b]/pvecback[pba->index_bg_rho_g];

    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddkappa] =
      -1./R*pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa];

    if(pba->has_idm_dr == _TRUE_) {

      /* - --> idr interaction rate with idm_dr (i.e. idr opacity to idm_dr scattering) */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dmu_idm_dr] =
        pth->a_idm_dr*pow((1.+pth->z_table[index_tau])/1.e7,pth->nindex_idm_dr)*pba->Omega0_idm_dr*pow(pba->h,2);

      /* - --> idm_dr interaction rate with idr (i.e. idm_dr opacity
               to idr scattering), [Sinv*dmu_idm_dr] with Sinv = (4
               rho_idr) / (3 rho_idm_dr), stored temporarily in
               ddmu_idm_dr */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddmu_idm_dr] =
        4./3.*pvecback[pba->index_bg_rho_idr]/pvecback[pba->index_bg_rho_idm_dr]
        *pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dmu_idm_dr];

      /* - --> idr self-interaction rate */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dmu_idr] =
        pth->b_idr*pow((1.+pth->z_table[index_tau])/1.e7,pth->nindex_idm_dr)*pba->Omega0_idr*pow(pba->h,2);
    }
  }

  /** - --> second derivative of this rate, -[1/R * kappa']'', stored temporarily in column dddkappa */
  class_call(array_spline_table_line_to_line(tau_table,
                                             pth->tt_size,
                                             pth->thermodynamics_table,
                                             pth->th_size,
                                             pth->index_th_ddkappa,
                                             pth->index_th_dddkappa,
                                             _SPLINE_EST_DERIV_,
                                             pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - --> compute tau_d = [int_{tau_today}^{tau} dtau -dkappa_d/dtau] */
  class_call(array_integrate_spline_table_line_to_line(tau_table,
                                                       pth->tt_size,
                                                       pth->thermodynamics_table,
                                                       pth->th_size,
                                                       pth->index_th_ddkappa,
                                                       pth->index_th_dddkappa,
                                                       pth->index_th_tau_d,
                                                       pth->error_message),
             pth->error_message,
             pth->error_message);

  /* the temporary quantities stored in columns ddkappa and dddkappa
     will not be used anymore, so they can be overwritten by other
     intermediate steps of other computations */

  if(pba->has_idm_dr == _TRUE_){

    /** --> second derivative of idm_dr interaction rate (with idr), [Sinv*dmu_idm_dr]'', stored temporarily in column dddmu */
    class_call(array_spline_table_line_to_line(tau_table,
                                               pth->tt_size,
                                               pth->thermodynamics_table,
                                               pth->th_size,
                                               pth->index_th_ddmu_idm_dr,
                                               pth->index_th_dddmu_idm_dr,
                                               _SPLINE_EST_DERIV_,
                                               pth->error_message),
               pth->error_message,
               pth->error_message);

    /** - --> compute optical depth of idm, tau_idm_dr = [int_{tau_today}^{tau} dtau [Sinv*dmu_idm_dr] ].
              This step gives -tau_idm_dr. The resulty is mutiplied by -1 later on. */
    class_call(array_integrate_spline_table_line_to_line(tau_table,
                                                         pth->tt_size,
                                                         pth->thermodynamics_table,
                                                         pth->th_size,
                                                         pth->index_th_ddmu_idm_dr,
                                                         pth->index_th_dddmu_idm_dr,
                                                         pth->index_th_tau_idm_dr,
                                                         pth->error_message),
               pth->error_message,
               pth->error_message);


    /** - --> second derivative of idr interaction rate (with idm_dr), [dmu_idm_idr]'', stored temporarily in column dddmu */
    class_call(array_spline_table_line_to_line(tau_table,
                                               pth->tt_size,
                                               pth->thermodynamics_table,
                                               pth->th_size,
                                               pth->index_th_dmu_idm_dr,
                                               pth->index_th_dddmu_idm_dr,
                                               _SPLINE_EST_DERIV_,
                                               pth->error_message),
               pth->error_message,
               pth->error_message);

    /** - --> compute optical depth of idr, tau_idr = [int_{tau_today}^{tau} dtau [dmu_idm_idr] ].
              This step gives -tau_idr. The resulty is mutiplied by -1 later on. */
    class_call(array_integrate_spline_table_line_to_line(tau_table,
                                                         pth->tt_size,
                                                         pth->thermodynamics_table,
                                                         pth->th_size,
                                                         pth->index_th_dmu_idm_dr,
                                                         pth->index_th_dddmu_idm_dr,
                                                         pth->index_th_tau_idr,
                                                         pth->error_message),
               pth->error_message,
               pth->error_message);
  }

  /** - --> compute damping scale:

      r_d = 2pi/k_d = 2pi * [int_{tau_ini}^{tau} dtau (1/kappa') 1/6 (R^2+16/15(1+R))/(1+R)^2]^1/2
      = 2pi * [int_{tau_ini}^{tau} dtau (1/kappa') 1/6 (R^2/(1+R)+16/15)/(1+R)]^1/2

      which is like in CosmoTherm (CT), but slightly
      different from Wayne Hu (WH)'s thesis eq. (5.59):
      the factor 16/15 in CT is 4/5 in WH */

  if (pth->compute_damping_scale == _TRUE_) {

    class_alloc(tau_table_growing,pth->tt_size*sizeof(double),pth->error_message);

    /* compute integrand and store temporarily in column "ddkappa" */
    for (index_tau=0; index_tau < pth->tt_size; index_tau++) {

      tau_table_growing[index_tau]=tau_table[pth->tt_size-1-index_tau];

      class_call(background_at_tau(pba,
                                   tau_table_growing[index_tau],
                                   pba->normal_info,
                                   pba->inter_closeby,
                                   &last_index_back,
                                   pvecback),
                 pba->error_message,
                 pth->error_message);

      R = 3./4.*pvecback[pba->index_bg_rho_b]/pvecback[pba->index_bg_rho_g];

      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddkappa] =
        1./6./pth->thermodynamics_table[(pth->tt_size-1-index_tau)*pth->th_size+pth->index_th_dkappa]
        *(R*R/(1+R)+16./15.)/(1.+R);

    }

    /* compute second derivative of integrand, and store temporarily in column "dddkappa" */
    class_call(array_spline_table_line_to_line(tau_table_growing,
                                               pth->tt_size,
                                               pth->thermodynamics_table,
                                               pth->th_size,
                                               pth->index_th_ddkappa,
                                               pth->index_th_dddkappa,
                                               _SPLINE_EST_DERIV_,
                                               pth->error_message),
               pth->error_message,
               pth->error_message);


    /* compute integral and store temporarily in column "g" */
    class_call(array_integrate_spline_table_line_to_line(tau_table_growing,
                                                         pth->tt_size,
                                                         pth->thermodynamics_table,
                                                         pth->th_size,
                                                         pth->index_th_ddkappa,
                                                         pth->index_th_dddkappa,
                                                         pth->index_th_g,
                                                         pth->error_message),
               pth->error_message,
               pth->error_message);

    free(tau_table_growing);

    /* we could now write the result as r_d = 2pi * sqrt(integral),
       but we will first better acount for the contribution frokm the tau_ini boundary.
       Close to this boundary, R=0 and the integrand is just 16/(15*6)/kappa'
       Using kappa' propto 1/a^2 and tau propro a during RD, we get the analytic result:
       int_0^{tau_ini} dtau / kappa' = tau_ini / 3 / kappa'_ini
       Thus r_d = 2pi * sqrt( 16/(15*6*3) * (tau_ini/ kappa'_ini) * integral) */

    tau_ini = tau_table[pth->tt_size-1];
    dkappa_ini = pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_dkappa];

    for (index_tau=0; index_tau < pth->tt_size; index_tau++) {

      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_r_d] =
        2.*_PI_*sqrt(16./(15.*6.*3.)*tau_ini/dkappa_ini
                     +pth->thermodynamics_table[(pth->tt_size-1-index_tau)*pth->th_size+pth->index_th_g]);
    }

  } // end of damping scale calculation

  /** - --> second derivative with respect to tau of dkappa (in view of spline interpolation) */
  class_call(array_spline_table_line_to_line(tau_table,
                                             pth->tt_size,
                                             pth->thermodynamics_table,
                                             pth->th_size,
                                             pth->index_th_dkappa,
                                             pth->index_th_dddkappa,
                                             _SPLINE_EST_DERIV_,
                                             pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - --> first derivative with respect to tau of dkappa (using spline interpolation) */
  class_call(array_derive_spline_table_line_to_line(tau_table,
                                                    pth->tt_size,
                                                    pth->thermodynamics_table,
                                                    pth->th_size,
                                                    pth->index_th_dkappa,
                                                    pth->index_th_dddkappa,
                                                    pth->index_th_ddkappa,
                                                    pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - --> compute -kappa = [int_{tau_today}^{tau} dtau dkappa/dtau], store temporarily in column "g" */
  class_call(array_integrate_spline_table_line_to_line(tau_table,
                                                       pth->tt_size,
                                                       pth->thermodynamics_table,
                                                       pth->th_size,
                                                       pth->index_th_dkappa,
                                                       pth->index_th_dddkappa,
                                                       pth->index_th_g,
                                                       pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - --> derivatives of baryon sound speed (only computed if some non-minimal tight-coupling schemes is requested) */
  if (pth->compute_cb2_derivatives == _TRUE_) {

    /** - ---> second derivative with respect to tau of cb2 */
    class_call(array_spline_table_line_to_line(tau_table,
                                               pth->tt_size,
                                               pth->thermodynamics_table,
                                               pth->th_size,
                                               pth->index_th_cb2,
                                               pth->index_th_ddcb2,
                                               _SPLINE_EST_DERIV_,
                                               pth->error_message),
               pth->error_message,
               pth->error_message);


    /** - ---> first derivative with respect to tau of cb2 (using spline interpolation) */
    class_call(array_derive_spline_table_line_to_line(tau_table,
                                                      pth->tt_size,
                                                      pth->thermodynamics_table,
                                                      pth->th_size,
                                                      pth->index_th_cb2,
                                                      pth->index_th_ddcb2,
                                                      pth->index_th_dcb2,
                                                      pth->error_message),
               pth->error_message,
               pth->error_message);
  }

  /** - --> compute visibility: \f$ g= (d \kappa/d \tau) e^{- \kappa} \f$ */

  /* loop on z (decreasing z, increasing time) */
  for (index_tau=pth->tt_size-1; index_tau>=0; index_tau--) {

    /** - ---> compute g */
    g = pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] *
      exp(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g]);

    /* for some very extreme models, in the last line, the exponential of a large negative number could go beyond the range covered by the "double" representation numbers, and be set to zero. To avoid a division by zero in the next steps, it is then better to set it to the minimum non-zero double (this has no impact on observables). */
    if (g==0.) g=DBL_MIN;

    /** - ---> compute exp(-kappa) */
    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_exp_m_kappa] =
      exp(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g]);

    /** - ---> compute g' (the plus sign of the second term is correct, see def of -kappa in thermodynamics module!) */
    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dg] =
      (pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddkappa] +
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] *
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa]) *
      exp(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g]);

    /** - ---> compute g''  */
    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddg] =
      (pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dddkappa] +
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] *
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddkappa] * 3. +
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] *
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] *
       pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa]) *
      exp(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g]);

    /** - ---> store g */
    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g] = g;

    /** - ---> compute variation rate */
    class_test(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa] == 0.,
               pth->error_message,
               "variation rate diverges");

    pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_rate] =
      sqrt(pow(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa],2)
           +pow(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddkappa]/
                pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa],2)
           +fabs(pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dddkappa]/
                 pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_dkappa]));

    /* - ---> restore correct sign for idm_dr and idr optical depth, and calculate idm_dr visibility function */
    if(pba->has_idm_dr == _TRUE_){

      /* restore the correct sign for tau_idm_dr */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_tau_idm_dr] *= -1.;

      /* restore the correct sign for tau_idr */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_tau_idr] *= -1.;

      /* visibility function for idm_dr : g_idm_dr = [Sinv*dmu_idm_dr] * exp(-tau_idm_dr) */
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g_idm_dr] =
        pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_ddmu_idm_dr]
        * exp(-pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_tau_idm_dr]);
    }
  }

  /** - smooth the rate (details of smoothing unimportant: only the
      order of magnitude of the rate matters) */
  class_call(array_smooth(pth->thermodynamics_table,
                          pth->th_size,
                          pth->tt_size,
                          pth->index_th_rate,
                          ppr->thermo_rate_smoothing_radius,
                          pth->error_message),
             pth->error_message,
             pth->error_message);

  /* - ---> fill columns for ddmu_idm_dr and dddmu_idm_dr with true values, and compute idm_dr temperature and sound speed */
  if(pba->has_idm_dr == _TRUE_){

    /** - --> second derivative with respect to tau of dmu_idm_dr (in view of spline interpolation) */
    class_call(array_spline_table_line_to_line(tau_table,
                                               pth->tt_size,
                                               pth->thermodynamics_table,
                                               pth->th_size,
                                               pth->index_th_dmu_idm_dr,
                                               pth->index_th_dddmu_idm_dr,
                                               _SPLINE_EST_DERIV_,
                                               pth->error_message),
               pth->error_message,
               pth->error_message);

    /** - --> first derivative with respect to tau of dmu_idm_dr (using spline interpolation) */
    class_call(array_derive_spline_table_line_to_line(tau_table,
                                                      pth->tt_size,
                                                      pth->thermodynamics_table,
                                                      pth->th_size,
                                                      pth->index_th_dmu_idm_dr,
                                                      pth->index_th_dddmu_idm_dr,
                                                      pth->index_th_ddmu_idm_dr,
                                                      pth->error_message),
               pth->error_message,
               pth->error_message);

    /** - --> now compute idm_dr temperature and sound speed in various regimes */

    /* (A) - initial value of T_idm_dr at the maximum z (minimum tau) */

    z = pth->z_table[pth->tt_size-1];

    class_call(background_tau_of_z(pba,z,&(tau)),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba,tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
               pba->error_message,
               pth->error_message);

    Gamma_heat_idm_dr = 2.*pba->Omega0_idr*pow(pba->h,2)*pth->a_idm_dr*pow((1.+z),(pth->nindex_idm_dr+1.))/pow(1.e7,pth->nindex_idm_dr);

    /* (A1) --> if Gamma is not much smaller than H, set T_idm_dr to T_idm_dr = T_idr = xi*T_gamma (tight coupling solution) */
    if(Gamma_heat_idm_dr > 1.e-3 * pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H]){
      T_idm_dr = pba->T_idr*(1.+z);
      dTdz_idm_dr = pba->T_idr;
    }

    /* (A2) --> otherwise, if Gamma << H, set initial T_idm_dr to the
       approximate analytic solution (Gamma/aH)/(1+(Gamma/aH)*T_idr)
       (eq. (A62) in ETHOS I ) */
    else {
      T_idr = pba->T_idr*(1.+z);
      T_idm_dr = Gamma_heat_idm_dr/(pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H])
        /(1. + Gamma_heat_idm_dr/(pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H]))*T_idr;
      dTdz_idm_dr = 2.*T_idm_dr - Gamma_heat_idm_dr/pvecback[pba->index_bg_H] * (T_idr - T_idm_dr);
    }

    pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_Tidm_dr] = T_idm_dr;
    pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_cidm_dr2] = _k_B_*T_idm_dr/_eV_/pth->m_idm*(1.+dTdz_idm_dr/3./T_idm_dr);

    /* T_adia and z_adia will be used later. They are defined as "the
       last T_idm_dr(z) at which the temperature was evaluated
       explicitely, rather than scaled like a^{-2} (decoupled DM
       regime)". Here we just initialize them. They will be updated
       each time that we recompte T_idm_dr explicitely. */
    T_adia = T_idm_dr;
    z_adia = z;

    /* (B) - iterate over growing tau / decreasing z to find other
       values. At each new z we need to compute the following
       quantities: T_idr, T_idm_dr, Gamma_heat_idm_dr, a, H, dT_idm_dr,/dz,
       c_s_idm_dr^2. They all needed to be known from step to step, even
       if the final goal is only to store T_idm_dr, c_s_idm^2 */
    for (index_tau=pth->tt_size-2; index_tau>=0; index_tau--) {

      /* (B1) --> tight-coupling solution: Gamma >> H implies T_idm_dr=T_idr=xi*T_gamma */
      if(Gamma_heat_idm_dr > 1.e3 * pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H]){
        z = pth->z_table[index_tau];
        T_idr = pba->T_idr*(1.+z);
        T_idm_dr = T_idr;
        Gamma_heat_idm_dr = 2.*pba->Omega0_idr*pow(pba->h,2)*pth->a_idm_dr*pow((1.+z),(pth->nindex_idm_dr+1.))/pow(1.e7,pth->nindex_idm_dr);
        class_call(background_tau_of_z(pba,z,&(tau)),
                   pba->error_message,
                   pth->error_message);
        class_call(background_at_tau(pba,tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                   pba->error_message,
                   pth->error_message);
        dTdz_idm_dr =pba->T_idr;
      }

      /* (B2) --> intermediate solution: integrate differential equation equation dT_idm_dr/dz = 2 a T_DM - Gamma/H (T_idr - T_idm_dr) */
      else if (Gamma_heat_idm_dr > 1.e-3 * pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H]) {

        dz = pth->z_table[index_tau+1] - pth->z_table[index_tau];

        /* (B2a) ----> if dz << H/Gamma the equation is not too stiff and the traditional forward Euler method converges */
        if (dz < pvecback[pba->index_bg_H]/Gamma_heat_idm_dr/10.) {
          z = pth->z_table[index_tau];
          T_idr = pba->T_idr*(1.+z);
          T_idm_dr -= dTdz_idm_dr*dz;
          Gamma_heat_idm_dr = 2.*pba->Omega0_idr*pow(pba->h,2)*pth->a_idm_dr*pow((1.+z),(pth->nindex_idm_dr+1.))/pow(1.e7,pth->nindex_idm_dr);
          class_call(background_tau_of_z(pba,z,&(tau)),
                     pba->error_message,
                     pth->error_message);
          class_call(background_at_tau(pba,tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                     pba->error_message,
                     pth->error_message);
          dTdz_idm_dr = 2.*pvecback[pba->index_bg_a]*T_idm_dr-Gamma_heat_idm_dr/(pvecback[pba->index_bg_H])*(T_idr-T_idm_dr);
        }

        /* (B2b) ----> otherwise, the equation is too stiff and the
           traditional forward Euler method diverges with this
           stepsize. But we can just decreasee dz to bring it back
           well within the convergence radius H/Gamma of the
           equation. */
        else {
          N_sub_steps = (int)(dz/ (pvecback[pba->index_bg_H]/Gamma_heat_idm_dr/10.))+1;
          dz_sub_step = dz/N_sub_steps;

          /* loop over sub-steps */
          for (n=0; n<N_sub_steps; n++) {

            /* evolve quantities over  sub-step wioth forward Euler method */

            z -= dz_sub_step;
            /* final redshift last sub-step overwritten to avoid small rounding error */
            if (n==(N_sub_steps-1)) z=pth->z_table[index_tau];

            T_idr = pba->T_idr*(1.+z);
            T_idm_dr -= dTdz_idm_dr*dz_sub_step;
            Gamma_heat_idm_dr = 2.*pba->Omega0_idr*pow(pba->h,2)*pth->a_idm_dr*pow((1.+z),(pth->nindex_idm_dr+1.))/pow(1.e7,pth->nindex_idm_dr);
            class_call(background_tau_of_z(pba,z,&(tau)),
                       pba->error_message,
                       pth->error_message);
            class_call(background_at_tau(pba,tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                       pba->error_message,
                       pth->error_message);
            dTdz_idm_dr = 2.*pvecback[pba->index_bg_a]*T_idm_dr-Gamma_heat_idm_dr/(pvecback[pba->index_bg_H])*(T_idr-T_idm_dr);
          }
        }

        /* update T_adia, z_adia */
        T_adia = T_idm_dr;
        z_adia = z;
      }

      /* (B3) --> decoupled solution: T_idm_dr scales like a^-2 */
      else {
        z = pth->z_table[index_tau];
        T_idr = pba->T_idr*(1.+z);
        T_idm_dr = T_adia * pow((1.+z)/(1.+z_adia),2);
        Gamma_heat_idm_dr = 2.*pba->Omega0_idr*pow(pba->h,2)*pth->a_idm_dr*pow((1.+z),(pth->nindex_idm_dr+1.))/pow(1.e7,pth->nindex_idm_dr);
        class_call(background_tau_of_z(pba,z,&(tau)),
                   pba->error_message,
                   pth->error_message);
        class_call(background_at_tau(pba,tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                   pba->error_message,
                   pth->error_message);
        dTdz_idm_dr = 2./(1+z)*T_idm_dr;
      }

      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_Tidm_dr] = T_idm_dr;
      pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_cidm_dr2] = _k_B_*T_idm_dr/_eV_/pth->m_idm*(1.+dTdz_idm_dr/3./T_idm_dr);
    }
  }

  free(tau_table);

  /** - fill tables of second derivatives with respect to z (in view of spline interpolation) */

  class_call(array_spline_table_lines(pth->z_table,
                                      pth->tt_size,
                                      pth->thermodynamics_table,
                                      pth->th_size,
                                      pth->d2thermodynamics_dz2_table,
                                      _SPLINE_EST_DERIV_,
                                      pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - find maximum of g */

  index_tau=pth->tt_size-1;
  while (pth->z_table[index_tau]>_Z_REC_MAX_) {
    index_tau--;
  }

  class_test(pth->thermodynamics_table[(index_tau+1)*pth->th_size+pth->index_th_g] >
             pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g],
             pth->error_message,
             "found a recombination redshift greater or equal to the maximum value imposed in thermodynamics.h, z_rec_max=%g",_Z_REC_MAX_);

  while (pth->thermodynamics_table[(index_tau+1)*pth->th_size+pth->index_th_g] <=
         pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g]) {
    index_tau--;
  }

  g_max = pth->thermodynamics_table[index_tau*pth->th_size+pth->index_th_g];
  index_tau_max = index_tau;

  /* approximation for maximum of g, using cubic interpolation, assuming equally spaced z's */
  pth->z_rec=pth->z_table[index_tau+1]+0.5*(pth->z_table[index_tau+1]-pth->z_table[index_tau])*(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_g]-pth->thermodynamics_table[(index_tau+2)*pth->th_size+pth->index_th_g])/(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_g]-2.*pth->thermodynamics_table[(index_tau+1)*pth->th_size+pth->index_th_g]+pth->thermodynamics_table[(index_tau+2)*pth->th_size+pth->index_th_g]);

  class_test(pth->z_rec+ppr->smallest_allowed_variation >= _Z_REC_MAX_,
             pth->error_message,
             "found a recombination redshift greater or equal to the maximum value imposed in thermodynamics.h, z_rec_max=%g",_Z_REC_MAX_);

  class_test(pth->z_rec-ppr->smallest_allowed_variation <= _Z_REC_MIN_,
             pth->error_message,
             "found a recombination redshift smaller or equal to the maximum value imposed in thermodynamics.h, z_rec_min=%g",_Z_REC_MIN_);

  /** - find conformal recombination time using background_tau_of_z() **/

  class_call(background_tau_of_z(pba,pth->z_rec,&(pth->tau_rec)),
             pba->error_message,
             pth->error_message);

  class_call(background_at_tau(pba,pth->tau_rec, pba->long_info, pba->inter_normal, &last_index_back, pvecback),
             pba->error_message,
             pth->error_message);

  pth->rs_rec=pvecback[pba->index_bg_rs];
  pth->ds_rec=pth->rs_rec*pba->a_today/(1.+pth->z_rec);
  pth->da_rec=pvecback[pba->index_bg_ang_distance];
  pth->ra_rec=pth->da_rec*(1.+pth->z_rec)/pba->a_today;
  pth->angular_rescaling=pth->ra_rec/(pba->conformal_age-pth->tau_rec);

  /** - find damping scale at recombination (using linear interpolation) */

  if (pth->compute_damping_scale == _TRUE_) {

    pth->rd_rec = (pth->z_table[index_tau+1]-pth->z_rec)/(pth->z_table[index_tau+1]-pth->z_table[index_tau])*pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_r_d]
      +(pth->z_rec-pth->z_table[index_tau])/(pth->z_table[index_tau+1]-pth->z_table[index_tau])*pth->thermodynamics_table[(index_tau+1)*pth->th_size+pth->index_th_r_d];

  }

  /** - find time (always after recombination) at which tau_c/tau
      falls below some threshold, defining tau_free_streaming */

  class_call(background_tau_of_z(pba,pth->z_table[index_tau],&tau),
             pba->error_message,
             pth->error_message);

  while ((1./pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_dkappa]/tau < ppr->radiation_streaming_trigger_tau_c_over_tau)
         && (index_tau>0)) {

    index_tau--;

    class_call(background_tau_of_z(pba,pth->z_table[index_tau],&tau),
               pba->error_message,
               pth->error_message);

  }

  pth->tau_free_streaming = tau;

  /** - Find interacting dark radiation free-streaming time */
  index_tau_fs = index_tau;

  if(pba->has_idr == _TRUE_) {

    if(pba->has_idm_dr == _TRUE_) {

      if(pth->nindex_idm_dr>=2){
        index_tau=index_tau_fs-1;
        /* comment: using index_tau_max (index_tau_fs) instead of pth->tt_size-1 ensures that the switch is always after recombination (free streaming) */
      }
      else{
        index_tau=0;
      }

      class_call(background_tau_of_z(pba,pth->z_table[index_tau],&tau),
                 pba->error_message,
                 pth->error_message);

      while ((1./pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_dmu_idm_dr]/tau
              < ppr->idr_streaming_trigger_tau_c_over_tau) &&
             ((pth->nindex_idm_dr >= 2 && index_tau > 0) ||
              (pth->nindex_idm_dr < 2 && index_tau < pth->tt_size-1))) {

        if(pth->nindex_idm_dr>=2){
          index_tau--;
        }
        else{
          index_tau++;
        }

        class_call(background_tau_of_z(pba,pth->z_table[index_tau],&tau),
                   pba->error_message,
                   pth->error_message);

      }

      tau_idm_dr_fs = tau;
      pth->tau_idr_free_streaming = tau;
    }

    /* case of idr alone without idm_dr */
    else {
      index_tau=index_tau_fs-1;
      class_call(background_tau_of_z(pba,pth->z_table[index_tau],&tau),
                 pba->error_message,
                 pth->error_message)
        tau_idm_dr_fs = tau;
      pth->tau_idr_free_streaming = tau;
    }
  }

  /** - find z_star (when optical depth kappa crosses one, using linear
      interpolation) and sound horizon at that time */

  index_tau=0;
  while ((pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_exp_m_kappa] > 1./_E_) && (index_tau < pth->tt_size))
    index_tau++;

  pth->z_star = pth->z_table[index_tau-1]+
    (1./_E_-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_exp_m_kappa])
    /(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_exp_m_kappa]-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_exp_m_kappa])
    *(pth->z_table[index_tau]-pth->z_table[index_tau-1]);

  class_call(background_tau_of_z(pba,pth->z_star,&(pth->tau_star)),
             pba->error_message,
             pth->error_message);

  class_call(background_at_tau(pba,pth->tau_star, pba->long_info, pba->inter_normal, &last_index_back, pvecback),
             pba->error_message,
             pth->error_message);

  pth->rs_star=pvecback[pba->index_bg_rs];
  pth->ds_star=pth->rs_star*pba->a_today/(1.+pth->z_star);
  pth->da_star=pvecback[pba->index_bg_ang_distance];
  pth->ra_star=pth->da_star*(1.+pth->z_star)/pba->a_today;

  if (pth->compute_damping_scale == _TRUE_) {

    pth->rd_star = (pth->z_table[index_tau+1]-pth->z_star)/(pth->z_table[index_tau+1]-pth->z_table[index_tau])*pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_r_d]
      +(pth->z_star-pth->z_table[index_tau])/(pth->z_table[index_tau+1]-pth->z_table[index_tau])*pth->thermodynamics_table[(index_tau+1)*pth->th_size+pth->index_th_r_d];

  }

  /** - find baryon drag time (when tau_d crosses one, using linear
      interpolation) and sound horizon at that time */

  index_tau=0;
  while ((pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_d] < 1.) && (index_tau < pth->tt_size))
    index_tau++;

  pth->z_d = pth->z_table[index_tau-1]+
    (1.-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_d])
    /(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_d]-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_d])
    *(pth->z_table[index_tau]-pth->z_table[index_tau-1]);

  class_call(background_tau_of_z(pba,pth->z_d,&(pth->tau_d)),
             pba->error_message,
             pth->error_message);

  class_call(background_at_tau(pba,pth->tau_d, pba->long_info, pba->inter_normal, &last_index_back, pvecback),
             pba->error_message,
             pth->error_message);

  pth->rs_d=pvecback[pba->index_bg_rs];
  pth->ds_d=pth->rs_d*pba->a_today/(1.+pth->z_d);

  /** - find idm_dr and idr drag times */
  if(pba->has_idm_dr == _TRUE_){

    if((pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idm_dr]>1.) && (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idr]>1.)){
      index_tau=0;

      while ((pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_idm_dr] < 1.) && (index_tau < pth->tt_size-1))
        index_tau++;

      z_idm_dr = pth->z_table[index_tau-1]+(1.-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_idm_dr])
        /(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_idm_dr]-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_idm_dr])
        *(pth->z_table[index_tau]-pth->z_table[index_tau-1]);

      class_call(background_tau_of_z(pba,z_idm_dr,&(tau_idm_dr)),
                 pba->error_message,
                 pth->error_message);

      index_tau=0;

      while ((pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_idr] < 1.) && (index_tau < pth->tt_size-1))
        index_tau++;

      z_idr = pth->z_table[index_tau-1]+
        (1.-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_idr])
        /(pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_tau_idr]-pth->thermodynamics_table[(index_tau-1)*pth->th_size+pth->index_th_tau_idr])
        *(pth->z_table[index_tau]-pth->z_table[index_tau-1]);

      class_call(background_tau_of_z(pba,z_idr,&(tau_idr)),
                 pba->error_message,
                 pth->error_message);
    }
  }

  /** - find time above which visibility falls below a given fraction of its maximum */

  index_tau=index_tau_max;
  while ((pth->thermodynamics_table[(index_tau)*pth->th_size+pth->index_th_g] >
          g_max * ppr->neglect_CMB_sources_below_visibility)
         && (index_tau > 0))
    index_tau--;

  class_call(background_tau_of_z(pba,pth->z_table[index_tau],&(pth->tau_cut)),
             pba->error_message,
             pth->error_message);

//MANUEL
  // Find the redshifts of thermal and kinetic decoupling.
  if (pth->compute_dm_thermo == _TRUE_) {

    // Computing the redshift of thermal decoupling
    my_index = pth->tt_size-1;//initial index

    class_call(background_tau_of_z(pba, pth->z_table[my_index], &my_tau),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba, my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
               pba->error_message,
               pth->error_message);

    aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];//conformal Hubble

    if (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_Rxprime]/aH >= 1.)
      starts_coupled = _TRUE_;
    else
      starts_coupled = _FALSE_;

    if (starts_coupled == _TRUE_) {
      while ((pth->thermodynamics_table[my_index*pth->th_size+pth->index_th_Rxprime]/aH >= 1.) && my_index>0) {

        my_index--;

        class_call(background_tau_of_z(pba,pth->z_table[my_index],&my_tau),
                   pba->error_message,
                   pth->error_message);

        class_call(background_at_tau(pba,my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                   pba->error_message,
                   pth->error_message);

        aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];
      }

      pth->z_thdec = pth->z_table[my_index+1];
    }
    else
      pth->z_thdec = -1;

    // Computing the redshift of kinetic decoupling
    my_index = pth->tt_size-1;//initial index

    class_call(background_tau_of_z(pba, pth->z_table[my_index], &my_tau),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba, my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
               pba->error_message,
               pth->error_message);

    aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];//conformal Hubble

    if (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_Rx]/aH >= 1.)
      starts_coupled = _TRUE_;
    else
      starts_coupled = _FALSE_;

    if (starts_coupled == _TRUE_) {
      while ((pth->thermodynamics_table[my_index*pth->th_size+pth->index_th_Rx]/aH >= 1.) && my_index>0) {

        my_index--;

        class_call(background_tau_of_z(pba, pth->z_table[my_index], &my_tau),
                   pba->error_message,
                   pth->error_message);

        class_call(background_at_tau(pba, my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                   pba->error_message,
                   pth->error_message);

        aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];
      }

      pth->z_tcdec = pth->z_table[my_index+1];
    }
    else
      pth->z_tcdec = -1;
  }
//MANUEL

  /** - if verbose flag set to next-to-minimum value, print the main results */

  if (pth->thermodynamics_verbose > 0) {

    if(pba->has_idm_dr == _TRUE_) {
      if((pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idm_dr]>1.) && (pth->thermodynamics_table[(pth->tt_size-1)*pth->th_size+pth->index_th_tau_idr]>1.)){
        printf(" -> idr decouples at tau_idr = %e Mpc\n",tau_idr);
        printf(" -> idm_dr decouples at tau_idm_dr = %e Mpc\n",tau_idm_dr);
      }
      else{
        printf(" -> computation of decoupling time of idm_dr and idr skipped, because z would not be in z_table\n");
      }
    }
    printf(" -> recombination at z = %f (max of visibility function)\n",pth->z_rec);
    printf("    corresponding to conformal time = %f Mpc\n",pth->tau_rec);
    printf("    with comoving sound horizon = %f Mpc\n",pth->rs_rec);
    printf("    angular diameter distance = %f Mpc\n",pth->da_rec);
    printf("    and sound horizon angle 100*theta_s = %f\n",100.*pth->rs_rec/pth->ra_rec);
    if (pth->compute_damping_scale == _TRUE_) {
      printf("    and with comoving photon damping scale = %f Mpc\n",pth->rd_rec);
      printf("    or comoving damping wavenumber k_d = %f 1/Mpc\n",2.*_PI_/pth->rd_rec);
    }
    printf("    Thomson optical depth crosses one at z_* = %f\n",pth->z_star);
    printf("    giving an angle 100*theta_* = %f\n",100.*pth->rs_star/pth->ra_star);
    printf(" -> baryon drag stops at z = %f\n",pth->z_d);
    printf("    corresponding to conformal time = %f Mpc\n",pth->tau_d);
    printf("    with comoving sound horizon rs = %f Mpc\n",pth->rs_d);
    if ((pth->reio_parametrization == reio_camb) || (pth->reio_parametrization == reio_half_tanh)) {
      if (pth->reio_z_or_tau==reio_tau)
        printf(" -> reionization  at z = %f\n",pth->z_reio);
      if (pth->reio_z_or_tau==reio_z)
        printf(" -> reionization with optical depth = %f\n",pth->tau_reio);
      class_call(background_tau_of_z(pba,pth->z_reio,&tau_reio),
                 pba->error_message,
                 pth->error_message);
      printf("    corresponding to conformal time = %f Mpc\n",tau_reio);
    }
    if (pth->reio_parametrization == reio_bins_tanh) {
      printf(" -> binned reionization gives optical depth = %f\n",pth->tau_reio);
    }
    if (pth->reio_parametrization == reio_many_tanh) {
      printf(" -> many-step reionization gives optical depth = %f\n",pth->tau_reio);
    }
    if (pth->reio_parametrization == reio_inter) {
      printf(" -> interpolated reionization history gives optical depth = %f\n",pth->tau_reio);
    }
    if (pth->thermodynamics_verbose > 1) {
      printf(" -> free-streaming approximation can be turned on as soon as tau=%g Mpc\n",
             pth->tau_free_streaming);
    }
    if ((pba->has_idr)&&(pth->thermodynamics_verbose > 1)) {
      printf(" -> dark free-streaming approximation can be turned on as soon as tau=%g Mpc\n",
             tau_idm_dr_fs);
    }
//MANUEL
    if (pth->compute_dm_thermo == _TRUE_) {

      printf("--------------------------------------------\nchi-b (DM-baryon interaction) parameters:\n    with interaction cross section sigma_chi-b = sigma_0 * v^n\n -> DM mass: mx = %g GeV\n -> DM-b cross section sigma_0 = %.2e cm^2\n -> v-power: n=%i, numeric coefficient: c_n=%f\n",pth->mx*1.e-3,pth->sigma_0*1.e4,(int)pth->pow_n,pth->c_n);
      printf(" -> non-relativistic redshift: z_nr = %.4e\n", pth->z_nr);

      if (pth->thermodynamics_verbose > 1) {

        class_call(background_tau_of_z(pba,pth->z_nr,&my_tau),
                   pba->error_message,
                   pth->error_message);

        class_call(background_at_tau(pba,my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                   pba->error_message,
                   pth->error_message);

        knr = 2.*pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

        printf("    wavenumber of comoving mode entering the horizon at that time: k_nr = %.4e 1/Mpc = %.4e h/Mpc\n", knr, knr/pba->h);
      }

      if (pth->pow_n > -3) {
        if ((pth->z_thdec >= 0.) && (pth->z_tcdec >= 0.)) {
          printf(" -> thermal decoupling redshift: z_thdec = %.4e\n -> kinetic decoupling redshift: z_tcdec = %.4e\n",pth->z_thdec,pth->z_tcdec);

          if (pth->thermodynamics_verbose > 1) {

            class_call(background_tau_of_z(pba,pth->z_thdec,&my_tau),
                       pba->error_message,
                       pth->error_message);

            class_call(background_at_tau(pba,my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                       pba->error_message,
                       pth->error_message);

            kthdec = 2.*pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

            class_call(background_tau_of_z(pba,pth->z_tcdec,&my_tau),
                       pba->error_message,
                       pth->error_message);

            class_call(background_at_tau(pba,my_tau, pba->short_info, pba->inter_normal, &last_index_back, pvecback),
                       pba->error_message,
                       pth->error_message);

            ktcdec = 2.*pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

            printf("    thermal decoupling wavenumber: k_thdec = %.4e 1/Mpc = %.4e h/Mpc\n", kthdec, kthdec/pba->h);
            printf("    kinetic decoupling wavenumber: k_tcdec = %.4e 1/Mpc = %.4e h/Mpc\n", ktcdec, ktcdec/pba->h);
          }
        }
        else
          printf(" -> chiDM is so weakly coupled that either the thermal (z_thdec=%g) and kinetic (z_tcdec=%g) decoupling redshifts happen very early in the Universe (above the largest redshift in the dmdecoupling routine, zmax=%g) and are irrelevant.\n",pth->z_thdec,pth->z_tcdec,ppr->z_dmde_max);
      }
      else if ((pth->pow_n <= -3) && (pth->realistic_Tx == _TRUE_))
        printf(" -> freeze-out cross section: <sigma v>_fo = %.2e cm^3/s\n -> freeze-out redshift: z_fo = %.4e\n",pth->sigmav_fo*1.e6,pth->z_fo);
    }
//MANUEL
  }

  free(pvecback);

  return _SUCCESS_;
}

/**
 * Free all memory space allocated by thermodynamics_init().
 *
 *
 * @param pth Input/Output: pointer to thermo structure (to be freed)
 * @return the error status
 */

int thermodynamics_free(
                        struct thermo * pth
                        ) {

  free(pth->z_table);
  free(pth->thermodynamics_table);
  free(pth->d2thermodynamics_dz2_table);

  return _SUCCESS_;
}

/**
 * Assign value to each relevant index in vectors of thermodynamical quantities,
 * as well as in vector containing reionization parameters.
 *
 *
 * @param pba   Input: pointer to background structure
 * @param pth   Input/Output: pointer to thermo structure
 * @param preco Input/Output: pointer to recombination structure
 * @param preio Input/Output: pointer to reionization structure
 * @return the error status
 */

int thermodynamics_indices(
                           struct background * pba,
                           struct thermo * pth,
                           struct recombination * preco,
                           struct reionization * preio,
                           struct dmdecoupling * pdmde//MANUEL: added the dmdecoupling structure
                           ) {

  /** Summary: */

  /** - define local variables */

  /* a running index for the vector of thermodynamics quantities */
  int index;

  /** - initialization of all indices and flags in thermo structure */
  index = 0;

  pth->index_th_xe = index;
  index++;
  pth->index_th_dkappa = index;
  index++;
  pth->index_th_tau_d = index;
  index++;
  pth->index_th_ddkappa = index;
  index++;
  pth->index_th_dddkappa = index;
  index++;
  pth->index_th_exp_m_kappa = index;
  index++;
  pth->index_th_g = index;
  index++;
  pth->index_th_dg = index;
  index++;
  pth->index_th_ddg = index;
  index++;
  pth->index_th_Tb = index;
  index++;
  pth->index_th_wb = index;
  index++;
  pth->index_th_cb2 = index;
  index++;
//MANUEL
  // - initialization of all indices and flags in thermo structure
  if (pth->compute_dm_thermo == _TRUE_) {
    pth->index_th_Tx = index;
    index++;
    pth->index_th_cx2 = index;
    index++;
    pth->index_th_Rx = index;
    index++;
    pth->index_th_Rxprime = index;
    index++;
    pth->index_th_dRx = index;
    index++;
  }
//MANUEL
  if(pba->has_idm_dr == _TRUE_){
    pth->index_th_dmu_idm_dr = index;
    index++;
    pth->index_th_ddmu_idm_dr = index;
    index++;
    pth->index_th_dddmu_idm_dr = index;
    index++;
    pth->index_th_tau_idm_dr = index;
    index++;
    pth->index_th_tau_idr = index;
    index++;
    pth->index_th_g_idm_dr = index;
    index++;
    pth->index_th_cidm_dr2 = index;
    index++;
    pth->index_th_Tidm_dr = index;
    index++;
    pth->index_th_dmu_idr = index;
    index++;
  }

  /* derivatives of baryon sound speed (only computed if some non-minimal tight-coupling schemes is requested) */
  if (pth->compute_cb2_derivatives == _TRUE_) {
    pth->index_th_dcb2 = index;
    index++;
    pth->index_th_ddcb2 = index;
    index++;
  }

  pth->index_th_rate = index;
  index++;

  if (pth->compute_damping_scale == _TRUE_) {
    pth->index_th_r_d = index;
    index++;
  }

  /* end of indices */
  pth->th_size = index;

//MANUEL
  // initialization of all indices and flags in dmdecoupling structure
  if (pth->compute_dm_thermo == _TRUE_) {
    index = 0;

    pdmde->index_dm_xe = index;
    index++;
    pdmde->index_dm_dkappadtau = index;
    index++;
    pdmde->index_dm_Tb = index;
    index++;
    pdmde->index_dm_wb = index;
    index++;
    pdmde->index_dm_cb2 = index;
    index++;
    pdmde->index_dm_Tx = index;
    index++;
    pdmde->index_dm_cx2 = index;
    index++;
    pdmde->index_dm_Rx = index;
    index++;
    pdmde->index_dm_Rxprime = index;
    index++;
    pdmde->index_dm_dRx = index;
    index++;

    // end of indices
    pdmde->dm_size = index;
  }
//MANUEL

  /** - initialization of all indices and flags in recombination structure */
  index = 0;

  preco->index_re_z = index;
  index++;
  preco->index_re_xe = index;
  index++;
  preco->index_re_dkappadtau = index;
  index++;
  preco->index_re_Tb = index;
  index++;
  preco->index_re_wb = index;
  index++;
  preco->index_re_cb2 = index;
  index++;
//MANUEL
  if (pth->compute_dm_thermo == _TRUE_) {
    preco->index_re_Tx = index;
    index++;
    preco->index_re_cx2 = index;
    index++;
    preco->index_re_Rx = index;
    index++;
    preco->index_re_Rxprime = index;
    index++;
    preco->index_re_dRx = index;
    index++;
  }
//MANUEL

  /* end of indices */
  preco->re_size = index;

  /** - initialization of all indices and flags in reionization structure */
  index = 0;

  preio->index_re_z = index;
  index++;
  preio->index_re_xe = index;
  index++;
  preio->index_re_Tb = index;
  index++;
  preio->index_re_wb = index;
  index++;
  preio->index_re_cb2 = index;
  index++;
  preio->index_re_dkappadtau = index;
  index++;
  preio->index_re_dkappadz = index;
  index++;
  preio->index_re_d3kappadz3 = index;
  index++;
//MANUEL
  if (pth->compute_dm_thermo == _TRUE_) {
    preio->index_re_Tx = index;
    index++;
    preio->index_re_cx2 = index;
    index++;
    preio->index_re_Rx = index;
    index++;
    preio->index_re_Rxprime = index;
    index++;
    preio->index_re_dRx = index;
    index++;
  }
//MANUEL

  /* end of indices */
  preio->re_size = index;

  /** - same with parameters of the function \f$ X_e(z)\f$ */

  index=0;

  preio->index_reio_start = index;
  index++;

  /* case where x_e(z) taken like in CAMB (other cases can be added) */
  if ((pth->reio_parametrization == reio_camb) || (pth->reio_parametrization == reio_half_tanh)) {

    preio->index_reio_redshift = index;
    index++;
    preio->index_reio_exponent = index;
    index++;
    preio->index_reio_width = index;
    index++;
    preio->index_reio_xe_before = index;
    index++;
    preio->index_reio_xe_after = index;
    index++;
    preio->index_helium_fullreio_fraction = index;
    index++;
    preio->index_helium_fullreio_redshift = index;
    index++;
    preio->index_helium_fullreio_width = index;
    index++;

  }

  /* case where x_e(z) is binned */
  if (pth->reio_parametrization == reio_bins_tanh) {

    /* the code will not only copy here the "bin centers" passed in
       input. It will add an initial and final value for (z,xe). So
       this array has a dimension bigger than the bin center array */

    preio->reio_num_z=pth->binned_reio_num+2; /* add two values: beginning and end of reio */

    preio->index_reio_first_z = index;
    index+= preio->reio_num_z;
    preio->index_reio_first_xe = index;
    index+= preio->reio_num_z;
    preio->index_reio_step_sharpness = index;
    index++;

  }

  /* case where x_e(z) has many tanh jumps */
  if (pth->reio_parametrization == reio_many_tanh) {

    /* the code will not only copy here the "jump centers" passed in
       input. It will add an initial and final value for (z,xe). So
       this array has a dimension bigger than the jump center array */

    preio->reio_num_z=pth->many_tanh_num+2; /* add two values: beginning and end of reio */

    preio->index_reio_first_z = index;
    index+= preio->reio_num_z;
    preio->index_reio_first_xe = index;
    index+= preio->reio_num_z;
    preio->index_reio_step_sharpness = index;
    index++;

  }

  /* case where x_e(z) must be interpolated */
  if (pth->reio_parametrization == reio_inter) {

    preio->reio_num_z=pth->reio_inter_num;

    preio->index_reio_first_z = index;
    index+= preio->reio_num_z;
    preio->index_reio_first_xe = index;
    index+= preio->reio_num_z;

  }

  preio->reio_num_params = index;

  /* flags for calling the interpolation routine */

  pth->inter_normal=0;
  pth->inter_closeby=1;

  return _SUCCESS_;
}

/**
 * Infer the primordial helium fraction from standard BBN, as a
 * function of the baryon density and expansion rate during BBN.
 *
 * This module is simpler then the one used in arXiv:0712.2826 because
 * it neglects the impact of a possible significant chemical
 * potentials for electron neutrinos. The full code with xi_nu_e could
 * be introduced here later.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param pth Input/Output: pointer to initialized thermo structure
 * @return the error status
 */
int thermodynamics_helium_from_bbn(
                                   struct precision * ppr,
                                   struct background * pba,
                                   struct thermo * pth
                                   ) {

  FILE * fA;
  char line[_LINE_LENGTH_MAX_];
  char * left;

  int num_omegab=0;
  int num_deltaN=0;

  double * omegab=NULL;
  double * deltaN=NULL;
  double * YHe=NULL;
  double * ddYHe=NULL;
  double * YHe_at_deltaN=NULL;
  double * ddYHe_at_deltaN=NULL;

  int array_line=0;
  double DeltaNeff;
  double omega_b;
  int last_index;
  double Neff_bbn, z_bbn, tau_bbn, *pvecback;

  /**Summary: */
  /** - Infer effective number of neutrinos at the time of BBN */
  class_alloc(pvecback,pba->bg_size*sizeof(double),pba->error_message);

  /** - 8.6173e-11 converts from Kelvin to MeV. We randomly choose 0.1 MeV to be the temperature of BBN */
  z_bbn = 0.1/(8.6173e-11*pba->T_cmb)-1.0;

  class_call(background_tau_of_z(pba,
                                 z_bbn,
                                 &tau_bbn),
             pba->error_message,
             pth->error_message);

  class_call(background_at_tau(pba,
                               tau_bbn,
                               pba->long_info,
                               pba->inter_normal,
                               &last_index,
                               pvecback),
             pba->error_message,
             pth->error_message);

  Neff_bbn = (pvecback[pba->index_bg_Omega_r]
              *pvecback[pba->index_bg_rho_crit]
              -pvecback[pba->index_bg_rho_g])
    /(7./8.*pow(4./11.,4./3.)*pvecback[pba->index_bg_rho_g]);

  free(pvecback);

  //  printf("Neff early = %g, Neff at bbn: %g\n",pba->Neff,Neff_bbn);

  /** - compute Delta N_eff as defined in bbn file, i.e. \f$ \Delta N_{eff}=0\f$ means \f$ N_{eff}=3.046\f$ */
  DeltaNeff = Neff_bbn - 3.046;

  /* the following file is assumed to contain (apart from comments and blank lines):
     - the two numbers (num_omegab, num_deltaN) = number of values of BBN free parameters
     - three columns (omegab, deltaN, YHe) where omegab = Omega0_b h^2 and deltaN = Neff-3.046 by definition
     - omegab and deltaN are assumed to be arranged as:
     omegab1 deltaN1 YHe
     omegab2 deltaN1 YHe
     .....
     omegab1 delatN2 YHe
     omegab2 deltaN2 YHe
     .....
  */

  class_open(fA,ppr->sBBN_file, "r",pth->error_message);

  /* go through each line */
  while (fgets(line,_LINE_LENGTH_MAX_-1,fA) != NULL) {

    /* eliminate blank spaces at beginning of line */
    left=line;
    while (left[0]==' ') {
      left++;
    }

    /* check that the line is neither blank neither a comment. In
       ASCII, left[0]>39 means that first non-blank character might
       be the beginning of some data (it is not a newline, a #, a %,
       etc.) */
    if (left[0] > 39) {

      /* if the line contains data, we must interpret it. If
         (num_omegab, num_deltaN)=(0,0), the current line must contain
         their values. Otherwise, it must contain (omegab, delatN,
         YHe). */
      if ((num_omegab==0) && (num_deltaN==0)) {

        /* read (num_omegab, num_deltaN), infer size of arrays and allocate them */
        class_test(sscanf(line,"%d %d",&num_omegab,&num_deltaN) != 2,
                   pth->error_message,
                   "could not read value of parameters (num_omegab,num_deltaN) in file %s\n",ppr->sBBN_file);

        class_alloc(omegab,num_omegab*sizeof(double),pth->error_message);
        class_alloc(deltaN,num_deltaN*sizeof(double),pth->error_message);
        class_alloc(YHe,num_omegab*num_deltaN*sizeof(double),pth->error_message);
        class_alloc(ddYHe,num_omegab*num_deltaN*sizeof(double),pth->error_message);
        class_alloc(YHe_at_deltaN,num_omegab*sizeof(double),pth->error_message);
        class_alloc(ddYHe_at_deltaN,num_omegab*sizeof(double),pth->error_message);
        array_line=0;

      }
      else {

        /* read (omegab, deltaN, YHe) */
        class_test(sscanf(line,"%lg %lg %lg",
                          &(omegab[array_line%num_omegab]),
                          &(deltaN[array_line/num_omegab]),
                          &(YHe[array_line])
                          ) != 3,
                   pth->error_message,
                   "could not read value of parameters (omegab,deltaN,YHe) in file %s\n",ppr->sBBN_file);
        array_line ++;
      }
    }
  }

  fclose(fA);

  /** - spline in one dimension (along deltaN) */
  class_call(array_spline_table_lines(deltaN,
                                      num_deltaN,
                                      YHe,
                                      num_omegab,
                                      ddYHe,
                                      _SPLINE_NATURAL_,
                                      pth->error_message),
             pth->error_message,
             pth->error_message);

  omega_b=pba->Omega0_b*pba->h*pba->h;

  class_test_except(omega_b < omegab[0],
                    pth->error_message,
                    free(omegab);free(deltaN);free(YHe);free(ddYHe);free(YHe_at_deltaN);free(ddYHe_at_deltaN),
                    "You have asked for an unrealistic small value omega_b = %e. The corresponding value of the primordial helium fraction cannot be found in the interpolation table. If you really want this value, you should fix YHe to a given value rather than to BBN",
                    omega_b);

  class_test_except(omega_b > omegab[num_omegab-1],
                    pth->error_message,
                    free(omegab);free(deltaN);free(YHe);free(ddYHe);free(YHe_at_deltaN);free(ddYHe_at_deltaN),
                    "You have asked for an unrealistic high value omega_b = %e. The corresponding value of the primordial helium fraction cannot be found in the interpolation table. If you really want this value, you should fix YHe to a given value rather than to BBN",
                    omega_b);

  class_test_except(DeltaNeff < deltaN[0],
                    pth->error_message,
                    free(omegab);free(deltaN);free(YHe);free(ddYHe);free(YHe_at_deltaN);free(ddYHe_at_deltaN),
                    "You have asked for an unrealistic small value of Delta N_eff = %e. The corresponding value of the primordial helium fraction cannot be found in the interpolation table. If you really want this value, you should fix YHe to a given value rather than to BBN",
                    DeltaNeff);

  class_test_except(DeltaNeff > deltaN[num_deltaN-1],
                    pth->error_message,
                    free(omegab);free(deltaN);free(YHe);free(ddYHe);free(YHe_at_deltaN);free(ddYHe_at_deltaN),
                    "You have asked for an unrealistic high value of Delta N_eff = %e. The corresponding value of the primordial helium fraction cannot be found in the interpolation table. If you really want this value, you should fix YHe to a given value rather than to BBN",
                    DeltaNeff);

  /** - interpolate in one dimension (along deltaN) */
  class_call(array_interpolate_spline(deltaN,
                                      num_deltaN,
                                      YHe,
                                      ddYHe,
                                      num_omegab,
                                      DeltaNeff,
                                      &last_index,
                                      YHe_at_deltaN,
                                      num_omegab,
                                      pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - spline in remaining dimension (along omegab) */
  class_call(array_spline_table_lines(omegab,
                                      num_omegab,
                                      YHe_at_deltaN,
                                      1,
                                      ddYHe_at_deltaN,
                                      _SPLINE_NATURAL_,
                                      pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - interpolate in remaining dimension (along omegab) */
  class_call(array_interpolate_spline(omegab,
                                      num_omegab,
                                      YHe_at_deltaN,
                                      ddYHe_at_deltaN,
                                      1,
                                      omega_b,
                                      &last_index,
                                      &(pth->YHe),
                                      1,
                                      pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - deallocate arrays */
  free(omegab);
  free(deltaN);
  free(YHe);
  free(ddYHe);
  free(YHe_at_deltaN);
  free(ddYHe_at_deltaN);

  return _SUCCESS_;

}

/**
 * In case of non-minimal cosmology, this function determines the
 * energy rate injected in the IGM at a given redshift z (= on-the-spot
 * annihilation). This energy injection may come e.g. from dark matter
 * annihilation or decay.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param preco Input: pointer to recombination structure
 * @param z Input: redshift
 * @param energy_rate Output: energy density injection rate
 * @param error_message Output: error message
 * @return the error status
 */

int thermodynamics_onthespot_energy_injection(
                                              struct precision * ppr,
                                              struct background * pba,
                                              struct recombination * preco,
                                              double z,
                                              double * energy_rate,
                                              ErrorMsg error_message
                                              ) {

  double annihilation_at_z;
  double rho_cdm_today;
  double u_min;
  double erfc;

  /*redshift-dependent annihilation parameter*/

  if (z>preco->annihilation_zmax) {

    annihilation_at_z = preco->annihilation*
      exp(-preco->annihilation_variation*pow(log((preco->annihilation_z+1.)/(preco->annihilation_zmax+1.)),2));
  }
  else if (z>preco->annihilation_zmin) {

    annihilation_at_z = preco->annihilation*
      exp(preco->annihilation_variation*(-pow(log((preco->annihilation_z+1.)/(preco->annihilation_zmax+1.)),2)
                                         +pow(log((z+1.)/(preco->annihilation_zmax+1.)),2)));
  }
  else {

    annihilation_at_z = preco->annihilation*
      exp(preco->annihilation_variation*(-pow(log((preco->annihilation_z+1.)/(preco->annihilation_zmax+1.)),2)
                                         +pow(log((preco->annihilation_zmin+1.)/(preco->annihilation_zmax+1.)),2)));
  }

  rho_cdm_today = pow(pba->H0*_c_/_Mpc_over_m_,2)*3/8./_PI_/_G_*(pba->Omega0_idm_dr+pba->Omega0_cdm)*_c_*_c_; /* energy density in J/m^3 */

  u_min = (1+z)/(1+preco->annihilation_z_halo);

  erfc = pow(1.+0.278393*u_min+0.230389*u_min*u_min+0.000972*u_min*u_min*u_min+0.078108*u_min*u_min*u_min*u_min,-4);

  *energy_rate = pow(rho_cdm_today,2)/_c_/_c_*pow((1+z),3)*
    (pow((1.+z),3)*annihilation_at_z+preco->annihilation_f_halo*erfc)
    +rho_cdm_today*pow((1+z),3)*preco->decay;
  /* energy density rate in J/m^3/s (remember that annihilation_at_z is in m^3/s/Kg and decay in s^-1) */

  return _SUCCESS_;

}

/**
 * In case of non-minimal cosmology, this function determines the
 * effective energy rate absorbed by the IGM at a given redshift
 * (beyond the on-the-spot annihilation). This energy injection may
 * come e.g. from dark matter annihilation or decay.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param preco Input: pointer to recombination structure
 * @param z Input: redshift
 * @param energy_rate Output: energy density injection rate
 * @param error_message Output: error message
 * @return the error status
 */

int thermodynamics_energy_injection(
                                    struct precision * ppr,
                                    struct background * pba,
                                    struct recombination * preco,
                                    double z,
                                    double * energy_rate,
                                    ErrorMsg error_message
                                    ) {

  double zp,dz;
  double integrand,first_integrand;
  double factor,result;
  double nH0;
  double onthespot;

  if (preco->annihilation > 0) {

    if (preco->has_on_the_spot == _FALSE_) {

      /* number of hydrogen nuclei today in m**-3 */
      nH0 = 3.*preco->H0*preco->H0*pba->Omega0_b/(8.*_PI_*_G_*_m_H_)*(1.-preco->YHe);

      /* factor = c sigma_T n_H(0) / (H(0) \sqrt(Omega_m)) (dimensionless) */
      factor = _sigma_ * nH0 / pba->H0 * _Mpc_over_m_ / sqrt(pba->Omega0_b+pba->Omega0_cdm+pba->Omega0_idm_dr+pba->Omega0_chi);//MANUEL

      /* integral over z'(=zp) with step dz */
      dz=1.;

      /* first point in trapezoidal integral */
      zp = z;
      class_call(thermodynamics_onthespot_energy_injection(ppr,pba,preco,zp,&onthespot,error_message),
                 error_message,
                 error_message);
      first_integrand = factor*pow(1+z,8)/pow(1+zp,7.5)*exp(2./3.*factor*(pow(1+z,1.5)-pow(1+zp,1.5)))*onthespot; // beware: versions before 2.4.3, there were wrong exponents: 6 and 5.5 instead of 8 and 7.5
      result = 0.5*dz*first_integrand;

      /* other points in trapezoidal integral */
      do {

        zp += dz;
        class_call(thermodynamics_onthespot_energy_injection(ppr,pba,preco,zp,&onthespot,error_message),
                   error_message,
                   error_message);
        integrand = factor*pow(1+z,8)/pow(1+zp,7.5)*exp(2./3.*factor*(pow(1+z,1.5)-pow(1+zp,1.5)))*onthespot; // beware: versions before 2.4.3, there were wrong exponents: 6 and 5.5 instead of 8 and 7.5
        result += dz*integrand;

      } while (integrand/first_integrand > 0.02);

      /* uncomment these lines if you also want to compute the on-the-spot for comparison */
      class_call(thermodynamics_onthespot_energy_injection(ppr,pba,preco,z,&onthespot,error_message),
                 error_message,
                 error_message);

    }
    else {
      class_call(thermodynamics_onthespot_energy_injection(ppr,pba,preco,z,&result,error_message),
                 error_message,
                 error_message);
    }

    /* these test lines print the energy rate rescaled by (1+z)^6 in J/m^3/s, with or without the on-the-spot approximation */
    /*
      fprintf(stdout,"%e  %e  %e \n",
      1.+z,
      result/pow(1.+z,6),
      onthespot/pow(1.+z,6));
    */

    /* effective energy density rate in J/m^3/s  */
    *energy_rate = result;

  }
  else {
    *energy_rate = 0.;
  }

  return _SUCCESS_;

}

/**
 * This subroutine contains the reionization function \f$ X_e(z) \f$
 * (one for each scheme; so far, only the function corresponding to
 * the reio_camb scheme is coded)
 *
 * @param z     Input: redshift
 * @param pth   Input: pointer to thermo structure, to know which scheme is used
 * @param preio Input: pointer to reionization structure, containing the parameters of the function \f$ X_e(z) \f$
 * @param xe    Output: \f$ X_e(z) \f$
 */

int thermodynamics_reionization_function(
                                         double z,
                                         struct thermo * pth,
                                         struct reionization * preio,
                                         double * xe
                                         ) {

  /** Summary: */

  /** - define local variables */
  double argument;
  int i;
  double z_jump;

  int jump;
  double center,before, after,width,one_jump;

  /** - implementation of ionization function similar to the one in CAMB */

  if ((pth->reio_parametrization == reio_camb) || (pth->reio_parametrization == reio_half_tanh)) {

    /** - --> case z > z_reio_start */

    if (z > preio->reionization_parameters[preio->index_reio_start]) {

      *xe = preio->reionization_parameters[preio->index_reio_xe_before];

    }

    else {

      /** - --> case z < z_reio_start: hydrogen contribution (tanh of complicated argument) */

      argument = (pow((1.+preio->reionization_parameters[preio->index_reio_redshift]),
                      preio->reionization_parameters[preio->index_reio_exponent])
                  - pow((1.+z),preio->reionization_parameters[preio->index_reio_exponent]))
        /(preio->reionization_parameters[preio->index_reio_exponent]
          /* no possible segmentation fault: checked to be non-zero in thermodynamics_reionization() */
          *pow((1.+preio->reionization_parameters[preio->index_reio_redshift]),
               (preio->reionization_parameters[preio->index_reio_exponent]-1.)))
        /preio->reionization_parameters[preio->index_reio_width];
      /* no possible segmentation fault: checked to be non-zero in thermodynamics_reionization() */

      if (pth->reio_parametrization == reio_camb) {
        *xe = (preio->reionization_parameters[preio->index_reio_xe_after]
               -preio->reionization_parameters[preio->index_reio_xe_before])
          *(tanh(argument)+1.)/2.
          +preio->reionization_parameters[preio->index_reio_xe_before];
      }
      else {
        *xe = (preio->reionization_parameters[preio->index_reio_xe_after]
               -preio->reionization_parameters[preio->index_reio_xe_before])
          *tanh(argument)
          +preio->reionization_parameters[preio->index_reio_xe_before];
      }

      /** - --> case z < z_reio_start: helium contribution (tanh of simpler argument) */

      if (pth->reio_parametrization == reio_camb) {
        argument = (preio->reionization_parameters[preio->index_helium_fullreio_redshift] - z)
          /preio->reionization_parameters[preio->index_helium_fullreio_width];
        /* no possible segmentation fault: checked to be non-zero in thermodynamics_reionization() */
        *xe += preio->reionization_parameters[preio->index_helium_fullreio_fraction]
          * (tanh(argument)+1.)/2.;
      }
    }

    return _SUCCESS_;

  }

  /** - implementation of binned ionization function similar to astro-ph/0606552 */

  if (pth->reio_parametrization == reio_bins_tanh) {

    /** - --> case z > z_reio_start */

    if (z > preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1]) {
      *xe = preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1];
    }

    else if (z < preio->reionization_parameters[preio->index_reio_first_z]) {
      *xe = preio->reionization_parameters[preio->index_reio_first_xe];
    }

    else {

      i = 0;
      while (preio->reionization_parameters[preio->index_reio_first_z+i+1]<z) i++;

      /* This is the expression of the tanh-like jumps of the
         reio_bins_tanh scheme until the 10.06.2015. It appeared to be
         not robust enough. It could lead to a kink in xe(z) near the
         maximum value of z at which reionisation is sampled. It has
         been replaced by the simpler and more robust expression
         below.

         *xe = preio->reionization_parameters[preio->index_reio_first_xe+i]
         +0.5*(tanh((2.*(z-preio->reionization_parameters[preio->index_reio_first_z+i])
         /(preio->reionization_parameters[preio->index_reio_first_z+i+1]
         -preio->reionization_parameters[preio->index_reio_first_z+i])-1.)
         /preio->reionization_parameters[preio->index_reio_step_sharpness])
         /tanh(1./preio->reionization_parameters[preio->index_reio_step_sharpness])+1.)
         *(preio->reionization_parameters[preio->index_reio_first_xe+i+1]
         -preio->reionization_parameters[preio->index_reio_first_xe+i]);
      */

      /* compute the central redshift value of the tanh jump */

      if (i == preio->reio_num_z-2) {
        z_jump = preio->reionization_parameters[preio->index_reio_first_z+i]
          + 0.5*(preio->reionization_parameters[preio->index_reio_first_z+i]
                 -preio->reionization_parameters[preio->index_reio_first_z+i-1]);
      }
      else  {
        z_jump =  0.5*(preio->reionization_parameters[preio->index_reio_first_z+i+1]
                       + preio->reionization_parameters[preio->index_reio_first_z+i]);
      }

      /* implementation of the tanh jump */

      *xe = preio->reionization_parameters[preio->index_reio_first_xe+i]
        +0.5*(tanh((z-z_jump)
                   /preio->reionization_parameters[preio->index_reio_step_sharpness])+1.)
        *(preio->reionization_parameters[preio->index_reio_first_xe+i+1]
          -preio->reionization_parameters[preio->index_reio_first_xe+i]);

    }

    return _SUCCESS_;

  }

  /** - implementation of many tanh jumps */

  if (pth->reio_parametrization == reio_many_tanh) {

    /** - --> case z > z_reio_start */

    if (z > preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1]) {
      *xe = preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1];
    }

    else if (z > preio->reionization_parameters[preio->index_reio_first_z]) {

      *xe = preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1];

      for (jump=1; jump<preio->reio_num_z-1; jump++){

        center = preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1-jump];
        // before and after are meant with respect to growing z, not growing time
        before = preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1-jump]
          -preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-jump];
        after = 0.;
        width = preio->reionization_parameters[preio->index_reio_step_sharpness];

        class_call(thermodynamics_tanh(z,center,before,after,width,&one_jump),
                   pth->error_message,
                   pth->error_message);

        *xe += one_jump;

      }

    }

    else {
      *xe = preio->reionization_parameters[preio->index_reio_first_xe];
    }

    return _SUCCESS_;

  }

  /** - implementation of reio_inter */

  if (pth->reio_parametrization == reio_inter) {

    /** - --> case z > z_reio_start */

    if (z > preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1]) {
      *xe = preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1];
      class_stop(pth->error_message,"Check: is it normal that we are here?");
    }

    else {

      i=0;
      while (preio->reionization_parameters[preio->index_reio_first_z+i+1] < z) i++;

      double z_min = preio->reionization_parameters[preio->index_reio_first_z+i];
      double z_max = preio->reionization_parameters[preio->index_reio_first_z+i+1];

      class_test(z<z_min,
                 pth->error_message,
                 "");

      class_test(z>z_max,
                 pth->error_message,
                 "");

      double x=(z-preio->reionization_parameters[preio->index_reio_first_z+i])
        /(preio->reionization_parameters[preio->index_reio_first_z+i+1]
          -preio->reionization_parameters[preio->index_reio_first_z+i]);

      *xe = preio->reionization_parameters[preio->index_reio_first_xe+i]
        + x*(preio->reionization_parameters[preio->index_reio_first_xe+i+1]
             -preio->reionization_parameters[preio->index_reio_first_xe+i]);

      class_test(*xe<0.,
                 pth->error_message,
                 "%e %e %e\n",
                 x,
                 preio->reionization_parameters[preio->index_reio_first_xe+i],
                 preio->reionization_parameters[preio->index_reio_first_xe+i+1]);

    }

    return _SUCCESS_;

  }

  class_test(0 == 0,
             pth->error_message,
             "value of reio_parametrization=%d unclear",pth->reio_parametrization);
}

/**
 * This subroutine reads \f$ X_e(z) \f$ in the recombination table at
 * the time at which reionization starts. Hence it provides correct
 * initial conditions for the reionization function.
 *
 * @param ppr   Input: pointer to precision structure
 * @param pth   Input: pointer to thermo structure
 * @param preco Input: pointer to recombination structure
 * @param z     Input: redshift z_reio_start
 * @param xe    Output: \f$ X_e(z) \f$ at z
 */

int thermodynamics_get_xe_before_reionization(
                                              struct precision * ppr,
                                              struct thermo * pth,
                                              struct recombination * preco,
                                              double z,
                                              double * xe
                                              ) {

  int last_index=0;

  class_call(array_interpolate_one_growing_closeby(preco->recombination_table,
                                                   preco->re_size,
                                                   preco->rt_size,
                                                   preco->index_re_z,
                                                   z,
                                                   &last_index,
                                                   preco->index_re_xe,
                                                   xe,
                                                   pth->error_message),
             pth->error_message,
             pth->error_message);

  return _SUCCESS_;

}


/**
 * This routine computes the reionization history. In the reio_camb
 * scheme, this is straightforward if the input parameter is the
 * reionization redshift. If the input is the optical depth, need to
 * find z_reio by dichotomy (trying several z_reio until the correct
 * tau_reio is approached).
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param pth Input: pointer to thermo structure
 * @param preco Input: pointer to filled recombination structure
 * @param preio Input/Output: pointer to reionization structure (to be filled)
 * @param pvecback   Input: vector of background quantities (used as workspace: must be already allocated, with format short_info or larger, but does not need to be filled)
 * @return the error status
 */

int thermodynamics_reionization(
                                struct precision * ppr,
                                struct background * pba,
                                struct thermo * pth,
                                struct recombination * preco,
                                struct reionization * preio,
                                struct dmdecoupling * pdmde,//MANUEL
                                struct thermo_vector * pthv,//MANUEL
                                double * pvecback
                                ) {

  /** Summary: */

  /** - define local variables */

  int counter;
  double z_sup,z_mid,z_inf;
  double tau_sup,tau_mid,tau_inf;
  int bin;
  int point;
  double xe_input,xe_actual;

  /** - allocate the vector of parameters defining the function \f$ X_e(z) \f$ */

  class_alloc(preio->reionization_parameters,preio->reio_num_params*sizeof(double),pth->error_message);

  /** - (a) if reionization implemented like in CAMB */

  if ((pth->reio_parametrization == reio_camb) || (pth->reio_parametrization == reio_half_tanh)) {

    /** - --> set values of these parameters, excepted those depending on the reionization redshift */

    if (pth->reio_parametrization == reio_camb) {
      preio->reionization_parameters[preio->index_reio_xe_after] = 1. + pth->YHe/(_not4_*(1.-pth->YHe));    /* xe_after_reio: H + singly ionized He (note: segmentation fault impossible, checked before that denominator is non-zero) */
    }
    if (pth->reio_parametrization == reio_half_tanh) {
      preio->reionization_parameters[preio->index_reio_xe_after] = 1.; /* xe_after_reio: neglect He ionization */
      //+ 2*pth->YHe/(_not4_*(1.-pth->YHe));    /* xe_after_reio: H + fully ionized He */
    }
    preio->reionization_parameters[preio->index_reio_exponent] = pth->reionization_exponent; /* reio_exponent */
    preio->reionization_parameters[preio->index_reio_width] = pth->reionization_width;    /* reio_width */
    preio->reionization_parameters[preio->index_helium_fullreio_fraction] = pth->YHe/(_not4_*(1.-pth->YHe)); /* helium_fullreio_fraction (note: segmentation fault impossible, checked before that denominator is non-zero) */
    preio->reionization_parameters[preio->index_helium_fullreio_redshift] = pth->helium_fullreio_redshift; /* helium_fullreio_redshift */
    preio->reionization_parameters[preio->index_helium_fullreio_width] = pth->helium_fullreio_width;    /* helium_fullreio_width */

    class_test(preio->reionization_parameters[preio->index_reio_exponent]==0,
               pth->error_message,
               "stop to avoid division by zero");

    class_test(preio->reionization_parameters[preio->index_reio_width]==0,
               pth->error_message,
               "stop to avoid division by zero");

    class_test(preio->reionization_parameters[preio->index_helium_fullreio_width]==0,
               pth->error_message,
               "stop to avoid division by zero");

    /** - --> if reionization redshift given as an input, initialize the remaining values and fill reionization table*/

    if (pth->reio_z_or_tau == reio_z) {

      /* reionization redshift */
      preio->reionization_parameters[preio->index_reio_redshift] = pth->z_reio;

      /* infer starting redshift for hydrogen */

      if (pth->reio_parametrization == reio_camb) {

        preio->reionization_parameters[preio->index_reio_start] = preio->reionization_parameters[preio->index_reio_redshift]+ppr->reionization_start_factor*pth->reionization_width;

        /* if starting redshift for helium is larger, take that one
           (does not happen in realistic models) */
        if (preio->reionization_parameters[preio->index_reio_start] <
            pth->helium_fullreio_redshift+ppr->reionization_start_factor*pth->helium_fullreio_width)

          preio->reionization_parameters[preio->index_reio_start] =
            pth->helium_fullreio_redshift+ppr->reionization_start_factor*pth->helium_fullreio_width;

      }
      else {

        preio->reionization_parameters[preio->index_reio_start] = pth->z_reio;
      }

      class_test(preio->reionization_parameters[preio->index_reio_start] > ppr->reionization_z_start_max,
                 pth->error_message,
                 "starting redshift for reionization > reionization_z_start_max = %e\n",ppr->reionization_z_start_max);

      /* infer xe_before_reio */
      class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                           pth,
                                                           preco,
                                                           preio->reionization_parameters[preio->index_reio_start],
                                                           &(preio->reionization_parameters[preio->index_reio_xe_before])),
                 pth->error_message,
                 pth->error_message);

      /* fill reionization table */
      class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
                 pth->error_message,
                 pth->error_message);

      pth->tau_reio=preio->reionization_optical_depth;

    }

    /** - --> if reionization optical depth given as an input, find reionization redshift by dichotomy and initialize the remaining values */

    if (pth->reio_z_or_tau == reio_tau) {

      /* upper value */

      z_sup = ppr->reionization_z_start_max-ppr->reionization_start_factor*pth->reionization_width;
      class_test(z_sup < 0.,
                 pth->error_message,
                 "parameters are such that reionization cannot take place before today while starting after z_start_max; need to increase z_start_max");

      /* maximum possible reionization redshift */
      preio->reionization_parameters[preio->index_reio_redshift] = z_sup;
      /* maximum possible starting redshift */
      preio->reionization_parameters[preio->index_reio_start] = ppr->reionization_z_start_max;
      /* infer xe_before_reio */
      class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                           pth,
                                                           preco,
                                                           preio->reionization_parameters[preio->index_reio_start],
                                                           &(preio->reionization_parameters[preio->index_reio_xe_before])),
                 pth->error_message,
                 pth->error_message);

      /* fill reionization table */
      class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),
                 pth->error_message,
                 pth->error_message);//MANUEL: added pdmde & pthv

      tau_sup=preio->reionization_optical_depth;

      class_test_except(tau_sup < pth->tau_reio,
                        pth->error_message,
                        free(preio->reionization_parameters);free(preio->reionization_table),
                        "parameters are such that reionization cannot start after z_start_max");

      /* lower value */

      z_inf = 0.;
      tau_inf = 0.;

      /* try intermediate values */

      counter=0;
      while ((tau_sup-tau_inf) > pth->tau_reio * ppr->reionization_optical_depth_tol) {
        z_mid=0.5*(z_sup+z_inf);

        /* reionization redshift */
        preio->reionization_parameters[preio->index_reio_redshift] = z_mid;
        /* infer starting redshift for hygrogen */
        preio->reionization_parameters[preio->index_reio_start] = preio->reionization_parameters[preio->index_reio_redshift]+ppr->reionization_start_factor*pth->reionization_width;
        /* if starting redshift for helium is larger, take that one
           (does not happen in realistic models) */
        if (preio->reionization_parameters[preio->index_reio_start] <
            pth->helium_fullreio_redshift+ppr->reionization_start_factor*pth->helium_fullreio_width)

          preio->reionization_parameters[preio->index_reio_start] =
            pth->helium_fullreio_redshift+ppr->reionization_start_factor*pth->helium_fullreio_width;

        class_test(preio->reionization_parameters[preio->index_reio_start] > ppr->reionization_z_start_max,
                   pth->error_message,
                   "starting redshift for reionization > reionization_z_start_max = %e",ppr->reionization_z_start_max);

        /* infer xe_before_reio */
        class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                             pth,
                                                             preco,
                                                             preio->reionization_parameters[preio->index_reio_start],
                                                             &(preio->reionization_parameters[preio->index_reio_xe_before])),
                   pth->error_message,
                   pth->error_message);

        /* clean and fill reionization table */
        free(preio->reionization_table);
        class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
                   pth->error_message,
                   pth->error_message);

        tau_mid=preio->reionization_optical_depth;

        /* trial */

        if (tau_mid > pth->tau_reio) {
          z_sup=z_mid;
          tau_sup=tau_mid;
        }
        else {
          z_inf=z_mid;
          tau_inf=tau_mid;
        }

        counter++;
        class_test(counter > _MAX_IT_,
                   pth->error_message,
                   "while searching for reionization_optical_depth, maximum number of iterations exceeded");
      }

      /* store z_reionization in thermodynamics structure */
      pth->z_reio=preio->reionization_parameters[preio->index_reio_redshift];

    }

    free(preio->reionization_parameters);

    return _SUCCESS_;

  }

  /** - (b) if reionization implemented with reio_bins_tanh scheme */

  if (pth->reio_parametrization == reio_bins_tanh) {

    /* this algorithm requires at least two bin centers (i.e. at least
       4 values in the (z,xe) array, counting the edges). */
    class_test(pth->binned_reio_num<2,
               pth->error_message,
               "current implementation of binned reio requires at least two bin centers");

    /* check that this input can be interpreted by the code */
    for (bin=1; bin<pth->binned_reio_num; bin++) {
      class_test(pth->binned_reio_z[bin-1]>=pth->binned_reio_z[bin],
                 pth->error_message,
                 "value of reionization bin centers z_i expected to be passed in growing order: %e, %e",
                 pth->binned_reio_z[bin-1],
                 pth->binned_reio_z[bin]);
    }

    /* the code will not only copy here the "bin centers" passed in
       input. It will add an initial and final value for (z,xe).
       First, fill all entries except the first and the last */

    for (bin=1; bin<preio->reio_num_z-1; bin++) {
      preio->reionization_parameters[preio->index_reio_first_z+bin] = pth->binned_reio_z[bin-1];
      preio->reionization_parameters[preio->index_reio_first_xe+bin] = pth->binned_reio_xe[bin-1];
    }


    /* find largest value of z in the array. We choose to define it as
       z_(i_max) + 2*(the distance between z_(i_max) and z_(i_max-1)). E.g. if
       the bins are in 10,12,14, the largest z will be 18. */
    preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1] =
      preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-2]
      +2.*(preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-2]
           -preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-3]);

    /* copy this value in reio_start */
    preio->reionization_parameters[preio->index_reio_start] = preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1];

    /* check it's not too big */
    class_test(preio->reionization_parameters[preio->index_reio_start] > ppr->reionization_z_start_max,
               pth->error_message,
               "starting redshift for reionization = %e, reionization_z_start_max = %e, you must change the binning or increase reionization_z_start_max",
               preio->reionization_parameters[preio->index_reio_start],
               ppr->reionization_z_start_max);

    /* find smallest value of z in the array. We choose
       to define it as z_0 - (the distance between z_1 and z_0). E.g. if
       the bins are in 10,12,14, the stop redshift will be 8. */

    preio->reionization_parameters[preio->index_reio_first_z] =
      2.*preio->reionization_parameters[preio->index_reio_first_z+1]
      -preio->reionization_parameters[preio->index_reio_first_z+2];

    /* check it's not too small */
    /* 6.06.2015: changed this test to simply imposing that the first z is at least zero */
    /*
      class_test(preio->reionization_parameters[preio->index_reio_first_z] < 0,
      pth->error_message,
      "final redshift for reionization = %e, you must change the binning or redefine the way in which the code extrapolates below the first value of z_i",preio->reionization_parameters[preio->index_reio_first_z]);
    */
    if (preio->reionization_parameters[preio->index_reio_first_z] < 0) {
      preio->reionization_parameters[preio->index_reio_first_z] = 0.;
    }

    /* infer xe before reio */
    class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                         pth,
                                                         preco,
                                                         preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1],
                                                         &(preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1])),
               pth->error_message,
               pth->error_message);

    /* infer xe after reio */
    preio->reionization_parameters[preio->index_reio_first_xe] = 1. + pth->YHe/(_not4_*(1.-pth->YHe));    /* xe_after_reio: H + singly ionized He (note: segmentation fault impossible, checked before that denominator is non-zero) */

    /* pass step sharpness parameter */
    preio->reionization_parameters[preio->index_reio_step_sharpness] = pth->binned_reio_step_sharpness;

    /* fill reionization table */
    class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
               pth->error_message,
               pth->error_message);

    pth->tau_reio=preio->reionization_optical_depth;

    return _SUCCESS_;

  }

  /** - (c) if reionization implemented with reio_many_tanh scheme */

  if (pth->reio_parametrization == reio_many_tanh) {

    /* this algorithm requires at least one jump centers */
    class_test(pth->many_tanh_num<1,
               pth->error_message,
               "current implementation of reio_many_tanh requires at least one jump center");

    /* check that z input can be interpreted by the code */
    for (bin=1; bin<pth->many_tanh_num; bin++) {
      class_test(pth->many_tanh_z[bin-1]>=pth->many_tanh_z[bin],
                 pth->error_message,
                 "value of reionization bin centers z_i expected to be passed in growing order: %e, %e",
                 pth->many_tanh_z[bin-1],
                 pth->many_tanh_z[bin]);
    }

    /* the code will not only copy here the "jump centers" passed in
       input. It will add an initial and final value for (z,xe).
       First, fill all entries except the first and the last */

    for (bin=1; bin<preio->reio_num_z-1; bin++) {

      preio->reionization_parameters[preio->index_reio_first_z+bin] = pth->many_tanh_z[bin-1];

      /* check that xe input can be interpreted by the code */
      xe_input = pth->many_tanh_xe[bin-1];
      if (xe_input >= 0.) {
        xe_actual = xe_input;
      }
      //-1 means "after hydrogen + first helium recombination"
      else if ((xe_input<-0.9) && (xe_input>-1.1)) {
        xe_actual = 1. + pth->YHe/(_not4_*(1.-pth->YHe));
      }
      //-2 means "after hydrogen + second helium recombination"
      else if ((xe_input<-1.9) && (xe_input>-2.1)) {
        xe_actual = 1. + 2.*pth->YHe/(_not4_*(1.-pth->YHe));
      }
      //other negative number is nonsense
      else {
        class_stop(pth->error_message,
                   "Your entry for many_tanh_xe[%d] is %e, this makes no sense (either positive or 0,-1,-2)",
                   bin-1,pth->many_tanh_xe[bin-1]);
      }

      preio->reionization_parameters[preio->index_reio_first_xe+bin] = xe_actual;
    }

    /* find largest value of z in the array. We choose to define it as
       z_(i_max) + ppr->reionization_start_factor*step_sharpness. */
    preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1] =
      preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-2]
      +ppr->reionization_start_factor*pth->many_tanh_width;

    /* copy this value in reio_start */
    preio->reionization_parameters[preio->index_reio_start] = preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1];

    /* check it's not too big */
    class_test(preio->reionization_parameters[preio->index_reio_start] > ppr->reionization_z_start_max,
               pth->error_message,
               "starting redshift for reionization = %e, reionization_z_start_max = %e, you must change the binning or increase reionization_z_start_max",
               preio->reionization_parameters[preio->index_reio_start],
               ppr->reionization_z_start_max);

    /* find smallest value of z in the array. We choose
       to define it as z_0 - ppr->reionization_start_factor*step_sharpness, but at least zero. */

    preio->reionization_parameters[preio->index_reio_first_z] =
      preio->reionization_parameters[preio->index_reio_first_z+1]
      -ppr->reionization_start_factor*pth->many_tanh_width;

    if (preio->reionization_parameters[preio->index_reio_first_z] < 0) {
      preio->reionization_parameters[preio->index_reio_first_z] = 0.;
    }

    /* infer xe before reio */
    class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                         pth,
                                                         preco,
                                                         preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1],
                                                         &(preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1])),
               pth->error_message,
               pth->error_message);

    /* infer xe after reio */

    preio->reionization_parameters[preio->index_reio_first_xe] = preio->reionization_parameters[preio->index_reio_first_xe+1];

    /* if we want to model only hydrogen reionization and neglect both helium reionization */
    //preio->reionization_parameters[preio->index_reio_first_xe] = 1.;

    /* if we want to model only hydrogen + first helium reionization and neglect second helium reionization */
    //preio->reionization_parameters[preio->index_reio_first_xe] = 1. + pth->YHe/(_not4_*(1.-pth->YHe));

    /* if we want to model hydrogen + two helium reionization */
    //preio->reionization_parameters[preio->index_reio_first_xe] = 1. + 2.*pth->YHe/(_not4_*(1.-pth->YHe));

    /* pass step sharpness parameter */
    class_test(pth->many_tanh_width<=0,
               pth->error_message,
               "many_tanh_width must be strictly positive, you passed %e",
               pth->many_tanh_width);

    preio->reionization_parameters[preio->index_reio_step_sharpness] = pth->many_tanh_width;

    /* fill reionization table */
    class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
               pth->error_message,
               pth->error_message);

    pth->tau_reio=preio->reionization_optical_depth;

    return _SUCCESS_;

  }

  /** - (d) if reionization implemented with reio_inter scheme */

  if (pth->reio_parametrization == reio_inter) {

    /* this parametrization requires at least one point (z,xe) */
    class_test(pth->reio_inter_num<1,
               pth->error_message,
               "current implementation of reio_inter requires at least one point (z,xe)");

    /* this parametrization requires that the first z value is zero */
    class_test(pth->reio_inter_z[0] != 0.,
               pth->error_message,
               "For reio_inter scheme, the first value of reio_inter_z[...]  should always be zero, you passed %e",
               pth->reio_inter_z[0]);

    /* check that z input can be interpreted by the code */
    for (point=1; point<pth->reio_inter_num; point++) {
      class_test(pth->reio_inter_z[point-1]>=pth->reio_inter_z[point],
                 pth->error_message,
                 "value of reionization bin centers z_i expected to be passed in growing order, unlike: %e, %e",
                 pth->reio_inter_z[point-1],
                 pth->reio_inter_z[point]);
    }

    /* this parametrization requires that the last x_i value is zero
       (the code will substitute it with the value that one would get in
       absence of reionization, as compute by the recombination code) */
    class_test(pth->reio_inter_xe[pth->reio_inter_num-1] != 0.,
               pth->error_message,
               "For reio_inter scheme, the last value of reio_inter_xe[...]  should always be zero, you passed %e",
               pth->reio_inter_xe[pth->reio_inter_num-1]);

    /* copy here the (z,xe) values passed in input. */

    for (point=0; point<preio->reio_num_z; point++) {

      preio->reionization_parameters[preio->index_reio_first_z+point] = pth->reio_inter_z[point];

      /* check that xe input can be interpreted by the code */
      xe_input = pth->reio_inter_xe[point];
      if (xe_input >= 0.) {
        xe_actual = xe_input;
      }
      //-1 means "after hydrogen + first helium recombination"
      else if ((xe_input<-0.9) && (xe_input>-1.1)) {
        xe_actual = 1. + pth->YHe/(_not4_*(1.-pth->YHe));
      }
      //-2 means "after hydrogen + second helium recombination"
      else if ((xe_input<-1.9) && (xe_input>-2.1)) {
        xe_actual = 1. + 2.*pth->YHe/(_not4_*(1.-pth->YHe));
      }
      //other negative number is nonsense
      else {
        class_stop(pth->error_message,
                   "Your entry for reio_inter_xe[%d] is %e, this makes no sense (either positive or 0,-1,-2)",
                   point,pth->reio_inter_xe[point]);
      }

      preio->reionization_parameters[preio->index_reio_first_xe+point] = xe_actual;
    }

    /* copy highest redshift in reio_start */
    preio->reionization_parameters[preio->index_reio_start] = preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1];

    /* check it's not too big */
    class_test(preio->reionization_parameters[preio->index_reio_start] > ppr->reionization_z_start_max,
               pth->error_message,
               "starting redshift for reionization = %e, reionization_z_start_max = %e, you must change the binning or increase reionization_z_start_max",
               preio->reionization_parameters[preio->index_reio_start],
               ppr->reionization_z_start_max);

    /* infer xe before reio */
    class_call(thermodynamics_get_xe_before_reionization(ppr,
                                                         pth,
                                                         preco,
                                                         preio->reionization_parameters[preio->index_reio_first_z+preio->reio_num_z-1],
                                                         &(preio->reionization_parameters[preio->index_reio_first_xe+preio->reio_num_z-1])),
               pth->error_message,
               pth->error_message);

    /* fill reionization table */
    class_call(thermodynamics_reionization_sample(ppr,pba,pth,preco,preio,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
               pth->error_message,
               pth->error_message);

    pth->tau_reio=preio->reionization_optical_depth;

    return _SUCCESS_;

  }

  class_test(0 == 0,
             pth->error_message,
             "value of reio_z_or_tau=%d unclear",pth->reio_z_or_tau);

}

/**
 * For fixed input reionization parameters, this routine computes the
 * reionization history and fills the reionization table.
 *
 * @param ppr Input: pointer to precision structure
 * @param pba Input: pointer to background structure
 * @param pth Input: pointer to thermo structure
 * @param preco Input: pointer to filled recombination structure
 * @param preio Input/Output: pointer to reionization structure (to be filled)
 * @param pvecback   Input: vector of background quantities (used as workspace: must be already allocated, with format short_info or larger, but does not need to be filled)
 * @return the error status
 */

int thermodynamics_reionization_sample(
                                       struct precision * ppr,
                                       struct background * pba,
                                       struct thermo * pth,
                                       struct recombination * preco,
                                       struct reionization * preio,
                                       struct dmdecoupling * pdmde,// MANUEL
                                       struct thermo_vector * pthv,//MANUEL
                                       double * pvecback
                                       ) {

  /** Summary: */

  /** - define local variables */

  /* a growing table (since the number of redshift steps is not known a priori) */
  growTable gTable;
  /* needed for growing table */
  double * pData;
  /* needed for growing table */
  void * memcopy_result;
  /* current vector of values related to reionization */
  double * reio_vector;
  /* running index inside thermodynamics table */
  int i;
  int number_of_redshifts;
  /* values of z, dz, X_e */
  double dz,dz_max;
  double z,z_next;
  double xe,xe_next;
  double dkappadz,dkappadz_next;
  double Tb,Yp,dTdz,opacity,mu;
  double dkappadtau,dkappadtau_next;
  double energy_rate;
  double tau;
  double chi_heat;
  int last_index_back;
  double relative_variation;
//MANUEL
  double V_rms2,dV2,Tba,Tex,dTxdz;
  double zstart,zend,tauv,tstart,tend;
//MANUEL

//MANUEL
  extern int thermo_stiff_ndf15();
  struct generic_integrator_workspace gi;

  struct thermodynamics_parameters_and_workspace tpaw;

  double y[2],dy[2];
  int index_thv;

  class_call(initialize_generic_integrator(2, &gi),
             gi.error_message,
             pth->error_message);
//MANUEL

  Yp = pth->YHe;

  /** - (a) allocate vector of values related to reionization */
  class_alloc(reio_vector,preio->re_size*sizeof(double),pth->error_message);

  /** - (b) create a growTable with gt_init() */
  class_call(gt_init(&gTable),
             gTable.error_message,
             pth->error_message);

  /** - (c) first line is taken from thermodynamics table, just before reionization starts */

  /** - --> look where to start in current thermodynamics table */
  i=0;
  while (preco->recombination_table[i*preco->re_size+preco->index_re_z] < preio->reionization_parameters[preio->index_reio_start]) {

    i++;
    class_test(i == ppr->recfast_Nz0,
               pth->error_message,
               "reionization_z_start_max = %e > largest redshift in thermodynamics table",ppr->reionization_z_start_max);
  }

  /** - --> get redshift */
  z=preco->recombination_table[i*preco->re_size+preco->index_re_z];
  reio_vector[preio->index_re_z]=z;
  preio->index_reco_when_reio_start=i;

  /** - --> get \f$ X_e \f$ */
  class_call(thermodynamics_reionization_function(z,pth,preio,&xe),
             pth->error_message,
             pth->error_message);

  reio_vector[preio->index_re_xe] = xe;

  /** -  --> get \f$ d \kappa / d z = (d \kappa / d \tau) * (d \tau / d z) = - (d \kappa / d \tau) / H \f$ */

  class_call(background_tau_of_z(pba,
                                 z,
                                 &tau),
             pba->error_message,
             pth->error_message);

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             pth->error_message);

  reio_vector[preio->index_re_dkappadtau] = (1.+z) * (1.+z) * pth->n_e * xe * _sigma_ * _Mpc_over_m_;

  class_test(pvecback[pba->index_bg_H] == 0.,
             pth->error_message,
             "stop to avoid division by zero");

  reio_vector[preio->index_re_dkappadz] = reio_vector[preio->index_re_dkappadtau] / pvecback[pba->index_bg_H];

  dkappadz = reio_vector[preio->index_re_dkappadz];
  dkappadtau = reio_vector[preio->index_re_dkappadtau];

  /** - --> get baryon temperature **/
  Tb = preco->recombination_table[i*preco->re_size+preco->index_re_Tb];
  reio_vector[preio->index_re_Tb] = Tb;

  /** - --> after recombination, Tb scales like (1+z)**2. Compute constant factor Tb/(1+z)**2. */
  //Tba2 = Tb/(1+z)/(1+z);

  /** - --> get baryon equation of state */
  reio_vector[preio->index_re_wb] = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * Yp + xe * (1.-Yp)) * Tb;

  /** - --> get baryon adiabatic sound speed */
  reio_vector[preio->index_re_cb2] = 5./3. * reio_vector[preio->index_re_wb];

//MANUEL
  /* get DM temperature, sound speed, and interaction rates */
  if (pth->compute_dm_thermo == _TRUE_) {

    Tex = preco->recombination_table[i*preco->re_size+preco->index_re_Tx];
    reio_vector[preio->index_re_Tx] = Tex;

    reio_vector[preio->index_re_cx2] = preco->recombination_table[i*preco->re_size+preco->index_re_cx2];
    reio_vector[preio->index_re_Rx] = preco->recombination_table[i*preco->re_size+preco->index_re_Rx];
    reio_vector[preio->index_re_Rxprime] = preco->recombination_table[i*preco->re_size+preco->index_re_Rxprime];
    reio_vector[preio->index_re_dRx] = preco->recombination_table[i*preco->re_size+preco->index_re_dRx];
  }
//MANUEL

  /** - --> store these values in growing table */
  class_call(gt_add(&gTable,_GT_END_,(void *) reio_vector,sizeof(double)*(preio->re_size)),
             gTable.error_message,
             pth->error_message);

  number_of_redshifts=1;

  /** - (d) set the maximum step value (equal to the step in thermodynamics table) */
  dz_max=preco->recombination_table[i*preco->re_size+preco->index_re_z]
    -preco->recombination_table[(i-1)*preco->re_size+preco->index_re_z];

  /** - (e) loop over redshift values in order to find values of z, x_e, kappa' (Tb and cb2 found later by integration). The sampling in z space is found here. */

  /* initial step */
  dz = dz_max;

  while (z > 0.) {

    class_test(dz < ppr->smallest_allowed_variation,
               pth->error_message,
               "stuck in the loop for reionization sampling, as if you were trying to impose a discontinuous evolution for xe(z)");

    /* - try next step */
    z_next=z-dz;
    if (z_next < 0.) z_next=0.;

    class_call(thermodynamics_reionization_function(z_next,pth,preio,&xe_next),
               pth->error_message,
               pth->error_message);

    class_call(background_tau_of_z(pba,
                                   z_next,
                                   &tau),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba,
                                 tau,
                                 pba->short_info,
                                 pba->inter_normal,
                                 &last_index_back,
                                 pvecback),
               pba->error_message,
               pth->error_message);

    class_test(pvecback[pba->index_bg_H] == 0.,
               pth->error_message,
               "stop to avoid division by zero");

    dkappadz_next= (1.+z_next) * (1.+z_next) * pth->n_e * xe_next * _sigma_ * _Mpc_over_m_ / pvecback[pba->index_bg_H];

    dkappadtau_next= (1.+z_next) * (1.+z_next) * pth->n_e * xe_next * _sigma_ * _Mpc_over_m_;

    class_test((dkappadz == 0.) || (dkappadtau == 0.),
               pth->error_message,
               "stop to avoid division by zero: dkappadz=%e, dkappadtau=%e",dkappadz,dkappadtau);//MANUEL: NOTE: modified what's being printed

    relative_variation = fabs((dkappadz_next-dkappadz)/dkappadz) +
      fabs((dkappadtau_next-dkappadtau)/dkappadtau);

    if (relative_variation < ppr->reionization_sampling) {
      /* accept the step: get \f$ z, X_e, d kappa / d z \f$ and store in growing table */

      z=z_next;
      xe=xe_next;
      dkappadz=dkappadz_next;
      dkappadtau= dkappadtau_next;

      class_test((dkappadz == 0.) || (dkappadtau == 0.),
                 pth->error_message,
                 "dkappadz=%e, dkappadtau=%e, stop to avoid division by zero",dkappadz,dkappadtau);

      reio_vector[preio->index_re_z] = z;
      reio_vector[preio->index_re_xe] = xe;
      reio_vector[preio->index_re_dkappadz] = dkappadz;
      reio_vector[preio->index_re_dkappadtau] = dkappadz * pvecback[pba->index_bg_H];

      class_call(gt_add(&gTable,_GT_END_,(void *) reio_vector,sizeof(double)*(preio->re_size)),
                 gTable.error_message,
                 pth->error_message);

      number_of_redshifts++;

      dz = MIN(0.9*(ppr->reionization_sampling/relative_variation),5.)*dz;
      dz = MIN(dz,dz_max);
    }
    else {
      /* do not accept the step and update dz */
      dz = 0.9*(ppr->reionization_sampling/relative_variation)*dz;
    }
  }

  /** - (f) allocate reionization_table with correct size */
  class_alloc(preio->reionization_table,preio->re_size*number_of_redshifts*sizeof(double),pth->error_message);

  preio->rt_size=number_of_redshifts;

  /** - (g) retrieve data stored in the growTable with gt_getPtr() */
  class_call(gt_getPtr(&gTable,(void**)&pData),
             gTable.error_message,
             pth->error_message);

  /** - (h) copy growTable to reionization_temporary_table (invert order of lines, so that redshift is growing, like in recombination table) */
  for (i=0; i < preio->rt_size; i++) {
    memcopy_result = memcpy(preio->reionization_table+i*preio->re_size,pData+(preio->rt_size-i-1)*preio->re_size,preio->re_size*sizeof(double));
    class_test(memcopy_result != preio->reionization_table+i*preio->re_size,
               pth->error_message,
               "cannot copy data back to reionization_temporary_table");

  }

  /** - (i) free the growTable with gt_free() , free vector of reionization variables */
  class_call(gt_free(&gTable),
             gTable.error_message,
             pth->error_message);

  free(reio_vector);

//MANUEL
  // memory allocation and conformal time table
  if ((pth->compute_dm_thermo == _TRUE_) && (pth->reio_integrator == stiff_int)) {

    class_alloc(pdmde->tau_vec,preio->rt_size*sizeof(double),pth->error_message);
    class_alloc(pthv,sizeof(struct thermo_vector),pth->error_message);

    //define the indices for pthv
    index_thv = 0;
    class_define_index(pthv->index_thv_Tb,_TRUE_,index_thv,1);
    class_define_index(pthv->index_thv_Tx,_TRUE_,index_thv,1);

    pthv->thv_size = index_thv;// = 2; clearly!!!!

    //allocate vectors for storing the values of all these quantities and their redshift-derivatives at a given redshift.
    class_calloc(pthv->y,pthv->thv_size,sizeof(double),pth->error_message);
    class_alloc(pthv->dy,pthv->thv_size*sizeof(double),pth->error_message);

    for (i=0; i<preio->rt_size; i++) {
      z = preio->reionization_table[(preio->rt_size-1-i)*preio->re_size+preio->index_re_z];

      class_call(background_tau_of_z(pba,
                                     z,
                                     &tauv),
                 pba->error_message,
                 pth->error_message);

      pdmde->tau_vec[i] = tauv;

    }
  }

  // avoiding cumbersome notation
  tpaw.pba = pba;
  tpaw.ppr = ppr;
  tpaw.preco = preco;
  tpaw.pvecback = pvecback;
  tpaw.pdmde = pdmde;

  if ((pth->compute_dm_thermo == _TRUE_) && (pth->reio_integrator == stiff_int))
    tpaw.pthv = pthv;
//MANUEL

  /** - (j) another loop on z, to integrate equation for Tb and to compute cb2 */
  for (i=preio->rt_size-1; i >0 ; i--) {

    z = preio->reionization_table[i*preio->re_size+preio->index_re_z];

    class_call(background_tau_of_z(pba,
                                   z,
                                   &tau),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba,
                                 tau,
                                 pba->normal_info,
                                 pba->inter_normal,
                                 &last_index_back,
                                 pvecback),
               pba->error_message,
               pth->error_message);

    dz = (preio->reionization_table[i*preio->re_size+preio->index_re_z]-preio->reionization_table[(i-1)*preio->re_size+preio->index_re_z]);

    opacity = (1.+z) * (1.+z) * pth->n_e
      * preio->reionization_table[i*preio->re_size+preio->index_re_xe] * _sigma_ * _Mpc_over_m_;

    mu = _m_H_/(1. + (1./_not4_ - 1.) * pth->YHe + preio->reionization_table[i*preio->re_size+preio->index_re_xe] * (1.-pth->YHe));

    /** - --> derivative of baryon temperature */

    dTdz=2./(1+z)*preio->reionization_table[i*preio->re_size+preio->index_re_Tb]
      -2.*mu/_m_e_*4.*pvecback[pba->index_bg_rho_g]/3./pvecback[pba->index_bg_rho_b]*opacity*
      (pba->T_cmb * (1.+z)-preio->reionization_table[i*preio->re_size+preio->index_re_Tb])/pvecback[pba->index_bg_H];

    if (preco->annihilation > 0) {

      class_call(thermodynamics_energy_injection(ppr,pba,preco,z,&energy_rate,pth->error_message),
                 pth->error_message,
                 pth->error_message);

      // old approximation from Chen and Kamionkowski:
      // chi_heat = (1.+2.*preio->reionization_table[i*preio->re_size+preio->index_re_xe])/3.;

      // coefficient as revised by Slatyer et al. 2013
      // (in fact it is a fit by Vivian Poulin of columns 1 and 2 in Table V
      // of Slatyer et al. 2013):
      xe = preio->reionization_table[i*preio->re_size+preio->index_re_xe];
      if (xe < 1.)
        chi_heat = MIN(0.996857*(1.-pow(1.-pow(xe,0.300134),1.51035)),1);
      else
        chi_heat = 1.;

      dTdz+= -2./(3.*_k_B_)*energy_rate*chi_heat
        /(preco->Nnow*pow(1.+z,3))/(1.+preco->fHe+preio->reionization_table[i*preio->re_size+preio->index_re_xe])
        /(pvecback[pba->index_bg_H]*_c_/_Mpc_over_m_*(1.+z)); /* energy injection */

    }

//MANUEL
//MANUEL: NOTE: next are the default routines, commented out, and placed inside some logic for compute_dm_thermo == _FALSE_ in the lines below
    // /** - --> increment baryon temperature */
    //
    // preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb] =
    //   preio->reionization_table[i*preio->re_size+preio->index_re_Tb]-dTdz*dz;
    //
    // /** - --> get baryon equation of state */
    //
    // preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb] =
    //   _k_B_/ ( _c_ * _c_ * mu)
    //   * preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb];
    //
    // /** - --> get baryon adiabatic sound speed */
    //
    // preio->reionization_table[(i-1)*preio->re_size+preio->index_re_cb2] =
    //   preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb]
    //   *(1.+(1+z)/3.*dTdz/preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb]);

    if (pth->compute_dm_thermo == _FALSE_) {//MANUEL: NOTE: put the default routines inside these parentheses

      /** - --> increment baryon temperature */

      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb] =
        preio->reionization_table[i*preio->re_size+preio->index_re_Tb]-dTdz*dz;

      /** - --> get baryon equation of state */

      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb] =
        _k_B_/ ( _c_ * _c_ * mu)
        * preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb];

      /** - --> get baryon adiabatic sound speed */

      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_cb2] =
        preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb]
        *(1.+(1+z)/3.*dTdz/preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb]);


    }

    else {//MANUEL: NOTE: the reionization module in the case of chiDM (IDM-b interactions)
      zend = preio->reionization_table[(i-1)*preio->re_size+preio->index_re_z];

      pdmde->temp_ne = pth->n_e;
      pdmde->temp_x = preio->reionization_table[(i-1)*preio->re_size+preio->index_re_xe];
      pdmde->temp_mu = _m_H_/(1. + (1./_not4_ - 1.) * pth->YHe + pdmde->temp_x * (1.-pth->YHe));

      y[0] = preio->reionization_table[i*preio->re_size+preio->index_re_Tb];
      y[1] = preio->reionization_table[i*preio->re_size+preio->index_re_Tx];

      zstart = z;
      tstart = tau;

      class_call(background_tau_of_z(pba,
                                     zend,
                                     &tend),
                 pba->error_message,
                 pth->error_message);

      if (pth->reio_integrator == stiff_int) {

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_reio,
                                      tstart,
                                      tend,
                                      y,
                                      2,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      preio->rt_size,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);
      }
      else if (pth->reio_integrator == generic_int) {

        class_call(generic_integrator(thermodynamics_z_derivs_reio,
                                      zstart,
                                      zend,
                                      y,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      &gi),
                   gi.error_message,
                   pth->error_message);
      }

      class_call(background_at_tau(pba,
                                   tend,
                                   pba->normal_info,
                                   pba->inter_normal,
                                   &last_index_back,
                                   pvecback),
                 pba->error_message,
                 pth->error_message);

      if (pth->reio_integrator == stiff_int) {

        Tba = pthv->y[pthv->index_thv_Tb];
        Tex = pthv->y[pthv->index_thv_Tx];

        dTdz = pthv->dy[pthv->index_thv_Tb]/(-pvecback[pba->index_bg_H]);
        dTxdz = pthv->dy[pthv->index_thv_Tx]/(-pvecback[pba->index_bg_H]);
      }
      else if (pth->reio_integrator == generic_int) {

        class_call(thermodynamics_z_derivs_reio(zend, y, dy, &tpaw, pth->error_message),
                   pth->error_message,
                   pth->error_message);

        Tba = y[0];
        Tex = y[1];

        dTdz = dy[0];
        dTxdz = dy[1];
      }

      if (pba->has_Vrms2 == _TRUE_) {
        if ((1./(1.+zend)) <= 1.e-3) {
          V_rms2 = pba->Vrms2;
          dV2 = 0.;
        }
        else {
          V_rms2 = pba->Vrms2 * pow(1.e-3*(1.+zend),2.);
          dV2 = 2.*V_rms2/(1.+zend);
        }
      }

      /** - --> increment baryon temperature */
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb] = Tba;

      /** - --> get baryon equation of state */
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb] =
        _k_B_/ ( _c_ * _c_ * pdmde->temp_mu)
        * preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb];

      /** - --> get baryon adiabatic sound speed */
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_cb2] =
        preio->reionization_table[(i-1)*preio->re_size+preio->index_re_wb]
        *(1.+(1+zend)/3.*dTdz/preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tb]);

      /** - --> increment DM temperature */
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Tx] = Tex;

      /** - --> DM adiabatic sound speed */
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_cx2] = pdmde->pref2 * pdmde->pref3 * (Tex + pth->pref_cx2*(1.+zend) * dTxdz / 3.);

      /**/
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Rx] = (1.+zend)*(1.+zend) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(Tba + pdmde->pref3*Tex), (pth->pow_n + 1.)/2.);

      /**/
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Rxprime] = pth->mxb * preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Rx];

      /**/
      preio->reionization_table[(i-1)*preio->re_size+preio->index_re_dRx] = -pvecback[pba->index_bg_H]*(2./(1.+zend) + ((pth->pow_n + 1.)/2.)*(dV2/3. + pdmde->pref2*(dTdz + pdmde->pref3*dTxdz))/(V_rms2/3. + pdmde->pref2*(Tba + pdmde->pref3*Tex)))*preio->reionization_table[(i-1)*preio->re_size+preio->index_re_Rx];
    }
  }
//MANUEL

  /** - --> spline \f$ d \tau / dz \f$ with respect to z in view of integrating for optical depth */
  class_call(array_spline(preio->reionization_table,
                          preio->re_size,
                          preio->rt_size,
                          preio->index_re_z,
                          preio->index_re_dkappadz,
                          preio->index_re_d3kappadz3,
                          _SPLINE_EST_DERIV_,
                          pth->error_message),
             pth->error_message,
             pth->error_message);

  /** - --> integrate for optical depth */
  class_call(array_integrate_all_spline(preio->reionization_table,
                                        preio->re_size,
                                        preio->rt_size,
                                        preio->index_re_z,
                                        preio->index_re_dkappadz,
                                        preio->index_re_d3kappadz3,
                                        &(preio->reionization_optical_depth),
                                        pth->error_message),
             pth->error_message,
             pth->error_message);

//MANUEL
  if (pth->compute_dm_thermo == _TRUE_) {

    if (pth->reio_integrator == stiff_int) {

      class_call(thermo_vector_free(pthv),
                 pth->error_message,
                 pth->error_message);

      free(pdmde->tau_vec);

      // MANUEL: NOTE: I assigned &gi earlier, even though never used it
      class_call(cleanup_generic_integrator(&gi),
                 gi.error_message,
                 pth->error_message);

    }
    else if (pth->reio_integrator == generic_int) {

      class_call(cleanup_generic_integrator(&gi),
                 gi.error_message,
                 pth->error_message);
    }
  }
//MANUEL

  return _SUCCESS_;

}

//MANUEL
/**
 * Subroutine evaluating the derivative with respect to conformal time of
 * thermodynamical quantities, for the reionization routine.
 *
 * Computes derivatives of the three variables to integrate: dTb/dtau, dTx/dtau.
 *
 * This is one of the few functions in the code which are passed to
 * the stiff integrator thermo_stiff_ndf15() routine.
 *
 * @param tau                      Input: conformal time
 * @param y                        Input: vector of variable to integrate
 * @param dy                       Output: its derivative (already allocated)
 * @param parameters_and_workspace Input: pointer to fixed parameters (e.g. indices) and workspace (already allocated)
 * @param error_message            Output: error message
 */

int thermodynamics_tau_derivs_reio(
                                   double tau,
                                   double * y,
                                   double * dy,
                                   void * parameters_and_workspace,
                                   ErrorMsg error_message
                                  ) {

  struct thermodynamics_parameters_and_workspace * ptpaw;
  struct precision * ppr;
  struct background * pba;
  struct recombination * preco;
  struct dmdecoupling * pdmde;

  double * pvecback;

  double z;
  int last_index_back;

  double S,mtot,Trad,Tmat,Tex;
  double V_rms2,aH,dkdt,Rx,Rxprime,Rg;

  ptpaw = parameters_and_workspace;
  ppr = ptpaw->ppr;
  pba = ptpaw->pba;
  preco = ptpaw->preco;
  pdmde = ptpaw->pdmde;
  pvecback = ptpaw->pvecback;

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  z = (1./pvecback[pba->index_bg_a]) - 1.;
  aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

  S = pba->Omega0_chi/pba->Omega0_b;
  mtot = (pdmde->dmx*_MeV_over_kg_)/pdmde->mxb;// [kg] total DM+b mass

  Trad = preco->Tnow * (1.+z);
  Tmat = y[0];
  Tex = y[1];

  if (pba->has_Vrms2 == _TRUE_) {
    if ((1./(1.+z)) <= 1.e-3)
      V_rms2 = pba->Vrms2;
    else
      V_rms2 = pba->Vrms2 * pow(1.e-3*(1.+z),2.);
  }

  dkdt = (1.+z) * (1.+z) * pdmde->temp_ne * pdmde->temp_x * _sigma_ * _Mpc_over_m_;
  Rg = (4./3.)*((1.+z)*pba->Omega0_g/pba->Omega0_b)*dkdt;

  Rx = (1.+z) * (1.+z) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(Tmat + pdmde->pref3*Tex), (pdmde->dpn + 1.)/2.);
  Rxprime = pdmde->mxb*Rx;

  dy[0] = -2*aH*Tmat + 2.*(pdmde->temp_mu/_m_e_)*Rg*(Trad-Tmat);

  if (pdmde->is_Tb_blind == _FALSE_)
    dy[0] += 2.*S*(pdmde->temp_mu/mtot)*Rx*(Tex-Tmat);

  dy[1] = -2.*aH*Tex + 2.*Rxprime*(Tmat-Tex);

  return _SUCCESS_;
}
//MANUEL

//MANUEL
int thermodynamics_z_derivs_reio(
                                   double z,
                                   double * y,
                                   double * dy,
                                   void * parameters_and_workspace,
                                   ErrorMsg error_message
                                  ) {

  struct thermodynamics_parameters_and_workspace * ptpaw;
  struct precision * ppr;
  struct background * pba;
  struct recombination * preco;
  struct dmdecoupling * pdmde;

  double * pvecback;

  double tau;
  int last_index_back;

  double H,Trad,Tmat,Tex,S,mtot,V_rms2;
  double dkdt,Rg,Rx,Rxprime;

  ptpaw = parameters_and_workspace;
  ppr = ptpaw->ppr;
  pba = ptpaw->pba;
  preco = ptpaw->preco;
  pdmde = ptpaw->pdmde;
  pvecback = ptpaw->pvecback;

  class_call(background_tau_of_z(pba,
                                 z,
                                 &tau),
             pba->error_message,
             error_message);

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  H = pvecback[pba->index_bg_H];

  Trad = preco->Tnow * (1.+z);
  Tmat = y[0];
  Tex = y[1];

  S = pba->Omega0_chi/pba->Omega0_b;
  mtot = (pdmde->dmx*_MeV_over_kg_)/pdmde->mxb;// [kg] total DM+b mass

  if (pba->has_Vrms2 == _TRUE_) {
    if ((1./(1.+z)) <= 1.e-3)
      V_rms2 = pba->Vrms2;
    else
      V_rms2 = pba->Vrms2 * pow(1.e-3*(1.+z),2.);
  }

  dkdt = (1.+z) * (1.+z) * pdmde->temp_ne * pdmde->temp_x * _sigma_ * _Mpc_over_m_;
  Rg = (4./3.)*((1.+z)*pba->Omega0_g/pba->Omega0_b)*dkdt;

  Rx = (1.+z) * (1.+z) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(Tmat + pdmde->pref3*Tex), (pdmde->dpn + 1.)/2.);
  Rxprime = pdmde->mxb*Rx;

  dy[0] = Tmat*2./(1.+z) - 2.*(pdmde->temp_mu/_m_e_)*(Rg/H)*(Trad-Tmat);

  if (pdmde->is_Tb_blind == _FALSE_)
    dy[0] -= 2.*S*(pdmde->temp_mu/mtot)*(Rx/H)*(Tex-Tmat);

  dy[1] = Tex*2./(1.+z) - 2.*(Rxprime/H)*(Tmat-Tex);

  return _SUCCESS_;
}
//MANUEL

/**
 * Integrate thermodynamics with your favorite recombination code.
 *
 */

int thermodynamics_recombination(
                                 struct precision * ppr,
                                 struct background * pba,
                                 struct thermo * pth,
                                 struct recombination * preco,
                                 struct dmdecoupling * pdmde,//MANUEL
                                 struct thermo_vector * pthv,//MANUEL
                                 double * pvecback
                                 ) {

  if (pth->recombination==hyrec) {

    class_call(thermodynamics_recombination_with_hyrec(ppr,pba,pth,preco,pvecback),
               pth->error_message,
               pth->error_message);

  }

  if (pth->recombination==recfast) {

    class_call(thermodynamics_recombination_with_recfast(ppr,pba,pth,preco,pdmde,pthv,pvecback),//MANUEL: added pdmde & pthv
               pth->error_message,
               pth->error_message);

  }

  return _SUCCESS_;

}

/**
 * Integrate thermodynamics with HyRec.
 *
 * Integrate thermodynamics with HyRec, allocate and fill the part
 * of the thermodynamics interpolation table (the rest is filled in
 * thermodynamics_init()). Called once by
 * thermodynamics_recombination(), from thermodynamics_init().
 *
 *************************************************************************************************
 *                 HYREC: Hydrogen and Helium Recombination Code
 *         Written by Yacine Ali-Haimoud and Chris Hirata (Caltech)
 *************************************************************************************************
 *
 *
 * @param ppr      Input: pointer to precision structure
 * @param pba      Input: pointer to background structure
 * @param pth      Input: pointer to thermodynamics structure
 * @param preco    Output: pointer to recombination structure
 * @param pvecback Input: pointer to an allocated (but empty) vector of background variables
 */

int thermodynamics_recombination_with_hyrec(
                                            struct precision * ppr,
                                            struct background * pba,
                                            struct thermo * pth,
                                            struct recombination * preco,
                                            double * pvecback
                                            ) {
  /** Summary: */
#ifdef HYREC

  REC_COSMOPARAMS param;
  HRATEEFF rate_table;
  TWO_PHOTON_PARAMS twog_params;
  double *xe_output, *Tm_output;
  int i,j,l,Nz,b;
  double z, xe, Tm, Hz;
  FILE *fA;
  FILE *fR;
  double L2s1s_current;
  void * buffer;
  int buf_size;
  double tau;
  int last_index_back;
  double w_fld,dw_over_da_fld,integral_fld;

  /** - Fill hyrec parameter structure */

  param.T0 = pba->T_cmb;
  param.obh2 = pba->Omega0_b*pba->h*pba->h;
  param.omh2 = (pba->Omega0_b+pba->Omega0_cdm+pba->Omega0_idm_dr+pba->Omega0_ncdm_tot+pba->Omega0_chi)*pba->h*pba->h;//MANUEL
  param.okh2 = pba->Omega0_k*pba->h*pba->h;
  param.odeh2 = (pba->Omega0_lambda+pba->Omega0_fld)*pba->h*pba->h;
  class_call(background_w_fld(pba,pba->a_today,&w_fld,&dw_over_da_fld,&integral_fld), pba->error_message, pth->error_message);
  param.w0 = w_fld;
  param.wa = -dw_over_da_fld*pba->a_today;
  param.Y = pth->YHe;
  param.Nnueff = pba->Neff;
  param.nH0 = 11.223846333047*param.obh2*(1.-param.Y);  /* number density of hydrogen today in m-3 */
  param.fHe = param.Y/(1-param.Y)/3.97153;              /* abundance of helium by number */
  param.zstart = ppr->recfast_z_initial; /* Redshift range */
  param.zend = 0.;
  param.dlna = 8.49e-5;
  param.nz = (long) floor(2+log((1.+param.zstart)/(1.+param.zend))/param.dlna);
  param.annihilation = pth->annihilation;
  param.has_on_the_spot = pth->has_on_the_spot;
  param.decay = pth->decay;
  param.annihilation_variation = pth->annihilation_variation;
  param.annihilation_z = pth->annihilation_z;
  param.annihilation_zmax = pth->annihilation_zmax;
  param.annihilation_zmin = pth->annihilation_zmin;
  param.annihilation_f_halo = pth->annihilation_f_halo;
  param.annihilation_z_halo = pth->annihilation_z_halo;

  /** - Build effective rate tables */

  /* allocate contiguous memory zone */

  buf_size = (2*NTR+NTM+2*NTR*NTM+2*param.nz)*sizeof(double) + 2*NTM*sizeof(double*);

  class_alloc(buffer,
              buf_size,
              pth->error_message);

  /** - distribute addresses for each table */

  rate_table.logTR_tab = (double*)buffer;
  rate_table.TM_TR_tab = (double*)(rate_table.logTR_tab + NTR);
  rate_table.logAlpha_tab[0] = (double**)(rate_table.TM_TR_tab+NTM);
  rate_table.logAlpha_tab[1] = (double**)(rate_table.logAlpha_tab[0]+NTM);
  rate_table.logAlpha_tab[0][0] = (double*)(rate_table.logAlpha_tab[1]+NTM);
  for (j=1;j<NTM;j++) {
    rate_table.logAlpha_tab[0][j] = (double*)(rate_table.logAlpha_tab[0][j-1]+NTR);
  }
  rate_table.logAlpha_tab[1][0] = (double*)(rate_table.logAlpha_tab[0][NTM-1]+NTR);
  for (j=1;j<NTM;j++) {
    rate_table.logAlpha_tab[1][j] = (double*)(rate_table.logAlpha_tab[1][j-1]+NTR);
  }
  rate_table.logR2p2s_tab = (double*)(rate_table.logAlpha_tab[1][NTM-1]+NTR);

  xe_output = (double*)(rate_table.logR2p2s_tab+NTR);
  Tm_output = (double*)(xe_output+param.nz);

  /* store sampled values of temperatures */

  for (i = 0; i < NTR; i++)
    rate_table.logTR_tab[i] = log(TR_MIN) + i * (log(TR_MAX)-log(TR_MIN))/(NTR-1.);
  for (i = 0; i < NTM; i++)
    rate_table.TM_TR_tab[i] = TM_TR_MIN + i * (TM_TR_MAX-TM_TR_MIN)/(NTM-1.);

  rate_table.DlogTR = rate_table.logTR_tab[1] - rate_table.logTR_tab[0];
  rate_table.DTM_TR = rate_table.TM_TR_tab[1] - rate_table.TM_TR_tab[0];

  /* read in file */

  class_open(fA,ppr->hyrec_Alpha_inf_file, "r",pth->error_message);
  class_open(fR,ppr->hyrec_R_inf_file, "r",pth->error_message);

  for (i = 0; i < NTR; i++) {
    for (j = 0; j < NTM; j++) {
      for (l = 0; l <= 1; l++) {
        if (fscanf(fA, "%le", &(rate_table.logAlpha_tab[l][j][i])) != 1)
          class_stop(pth->error_message,"Error reading hyrec data file %s",ppr->hyrec_Alpha_inf_file);
        rate_table.logAlpha_tab[l][j][i] = log(rate_table.logAlpha_tab[l][j][i]);
      }
    }

    if (fscanf(fR, "%le", &(rate_table.logR2p2s_tab[i])) !=1)
      class_stop(pth->error_message,"Error reading hyrec data file %s",ppr->hyrec_R_inf_file);
    rate_table.logR2p2s_tab[i] = log(rate_table.logR2p2s_tab[i]);

  }
  fclose(fA);
  fclose(fR);

  /* Read two-photon rate tables */

  class_open(fA,ppr->hyrec_two_photon_tables_file, "r",pth->error_message);

  for (b = 0; b < NVIRT; b++) {
    if ((fscanf(fA, "%le", &(twog_params.Eb_tab[b])) != 1) ||
        (fscanf(fA, "%le", &(twog_params.A1s_tab[b])) != 1) ||
        (fscanf(fA, "%le", &(twog_params.A2s_tab[b])) != 1) ||
        (fscanf(fA, "%le", &(twog_params.A3s3d_tab[b])) != 1) ||
        (fscanf(fA, "%le", &(twog_params.A4s4d_tab[b])) != 1))
      class_stop(pth->error_message,"Error reading hyrec data file %s",ppr->hyrec_two_photon_tables_file);
  }

  fclose(fA);

  /** - Normalize 2s--1s differential decay rate to L2s1s (can be set by user in hydrogen.h) */
  L2s1s_current = 0.;
  for (b = 0; b < NSUBLYA; b++) L2s1s_current += twog_params.A2s_tab[b];
  for (b = 0; b < NSUBLYA; b++) twog_params.A2s_tab[b] *= L2s1s/L2s1s_current;

  /*  In CLASS, we have neutralized the switches for the various
      effects considered in Hirata (2008), keeping the full
      calculation as a default; but you could restore their
      functionality by copying a few lines from hyrec/hyrec.c to
      here */

  /** - Compute the recombination history by calling a function in hyrec (no CLASS-like error management here) */

  if (pth->thermodynamics_verbose > 0)
    printf(" -> calling HyRec version %s,\n",HYREC_VERSION);

  rec_build_history(&param, &rate_table, &twog_params, xe_output, Tm_output);

  if (pth->thermodynamics_verbose > 0)
    printf("    by Y. Ali-Haïmoud & C. Hirata\n");

  /** - fill a few parameters in preco and pth */

  Nz=ppr->recfast_Nz0;

  preco->rt_size = Nz;
  preco->H0 = pba->H0 * _c_ / _Mpc_over_m_;
  /* preco->H0 in inverse seconds (while pba->H0 is [H0/c] in inverse Mpcs) */
  preco->YHe = pth->YHe;
  preco->Nnow = 3.*preco->H0*preco->H0*pba->Omega0_b*(1.-preco->YHe)/(8.*_PI_*_G_*_m_H_);
  /* energy injection parameters */
  preco->annihilation = pth->annihilation;
  preco->has_on_the_spot = pth->has_on_the_spot;
  preco->annihilation_variation = pth->annihilation_variation;
  preco->annihilation_z = pth->annihilation_z;
  preco->annihilation_zmax = pth->annihilation_zmax;
  preco->annihilation_zmin = pth->annihilation_zmin;
  preco->decay = pth->decay;
  preco->annihilation_f_halo = pth->annihilation_f_halo;
  preco->annihilation_z_halo = pth->annihilation_z_halo;
  pth->n_e=preco->Nnow;

  /** - allocate memory for thermodynamics interpolation tables (size known in advance) and fill it */

  class_alloc(preco->recombination_table,preco->re_size*preco->rt_size*sizeof(double),pth->error_message);

  for(i=0; i <Nz; i++) {

    /** - --> get redshift, corresponding results from hyrec, and background quantities */

    z = param.zstart * (1. - (double)(i+1) / (double)Nz);

    /* get (xe,Tm) by interpolating in pre-computed tables */


    class_call(array_interpolate_cubic_equal(-log(1.+param.zstart),
                                             param.dlna,
                                             xe_output,
                                             param.nz,
                                             -log(1.+z),
                                             &xe,
                                             pth->error_message),
               pth->error_message,
               pth->error_message);

    class_call(array_interpolate_cubic_equal(-log(1.+param.zstart),
                                             param.dlna,
                                             Tm_output,
                                             param.nz,
                                             -log(1.+z),
                                             &Tm,
                                             pth->error_message),
               pth->error_message,
               pth->error_message);

    class_call(background_tau_of_z(pba,
                                   z,
                                   &tau),
               pba->error_message,
               pth->error_message);

    class_call(background_at_tau(pba,
                                 tau,
                                 pba->short_info,
                                 pba->inter_normal,
                                 &last_index_back,
                                 pvecback),
               pba->error_message,
               pth->error_message);

    /*   class_call(thermodynamics_energy_injection(ppr,pba,preco,z,&energy_rate,pth->error_message),
         pth->error_message,
         pth->error_message);
    */

    /* Hz is H in inverse seconds (while pvecback returns [H0/c] in inverse Mpcs) */
    Hz=pvecback[pba->index_bg_H] * _c_ / _Mpc_over_m_;

    /** - --> store the results in the table */

    /* results are obtained in order of decreasing z, and stored in order of growing z */

    /* redshift */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z)=z;

    /* ionization fraction */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe)=xe;

    /* Tb */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb)=Tm;

    /* wb = (k_B/mu) Tb */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
      = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * pth->YHe + xe * (1.-pth->YHe)) * Tm;

    /* cb2 = (k_B/mu) Tb (1-1/3 dlnTb/dlna) = (k_B/mu) Tb (1+1/3 (1+z) dlnTb/dz)
       with (1+z)dlnTb/dz= - [dlnTb/dlna] */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2)
      = *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
      * (1. - rec_dTmdlna(xe, Tm, pba->T_cmb*(1.+z), Hz, param.fHe, param.nH0*pow((1+z),3)*1e-6, energy_injection_rate(&param,z)) / Tm / 3.);

    /* dkappa/dtau = a n_e x_e sigma_T = a^{-2} n_e(today) x_e sigma_T (in units of 1/Mpc) */
    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau)
      = (1.+z) * (1.+z) * preco->Nnow * xe * _sigma_ * _Mpc_over_m_;

  }

  /* Cleanup */

  free(buffer);

#else

  class_stop(pth->error_message,
             "you compiled without including the HyRec code, and now wish to use it. Either set the input parameter 'recombination' to something else than 'HyRec', or recompile after setting in the Makefile the appropriate path HYREC=... ");

#endif

  return _SUCCESS_;
}

/**
 * Integrate thermodynamics with RECFAST.
 *
 * Integrate thermodynamics with RECFAST, allocate and fill the part
 * of the thermodynamics interpolation table (the rest is filled in
 * thermodynamics_init()). Called once by
 * thermodynamics_recombination, from thermodynamics_init().
 *
 *
 *******************************************************************************
 * RECFAST is an integrator for Cosmic Recombination of Hydrogen and Helium,
 * developed by Douglas Scott (dscott@astro.ubc.ca)
 * based on calculations in the paper Seager, Sasselov & Scott
 * (ApJ, 523, L1, 1999).
 * and "fudge" updates in Wong, Moss & Scott (2008).
 *
 * Permission to use, copy, modify and distribute without fee or royalty at
 * any tier, this software and its documentation, for any purpose and without
 * fee or royalty is hereby granted, provided that you agree to comply with
 * the following copyright notice and statements, including the disclaimer,
 * and that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for distribution:
 *
 * Copyright 1999-2010 by University of British Columbia.  All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND U.B.C. MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.
 * BY WAY OF EXAMPLE, BUT NOT LIMITATION,
 * U.B.C. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT
 * THE USE OF THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE
 * ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *******************************************************************************
 *
 * Version 1.5: includes extra fitting function from
 *              Rubino-Martin et al. arXiv:0910.4383v1 [astro-ph.CO]
 *
 * @param ppr      Input: pointer to precision structure
 * @param pba      Input: pointer to background structure
 * @param pth      Input: pointer to thermodynamics structure
 * @param preco    Output: pointer to recombination structure
 * @param pvecback Input: pointer to an allocated (but empty) vector of background variables
 * @return the error status
 */

int thermodynamics_recombination_with_recfast(
                                              struct precision * ppr,
                                              struct background * pba,
                                              struct thermo * pth,
                                              struct recombination * preco,
                                              struct dmdecoupling * pdmde,//MANUEL
                                              struct thermo_vector * pthv,//MANUEL
                                              double * pvecback
                                              ) {

  /** Summary: */

  /** - define local variables */
  // /* vector of variables to be integrated: x_H, x_He, Tmat */
  // double y[3],dy[3];//MANUEL: ORIGINAL
//MANUEL
  /* vector of variables to be integrated: x_H, x_He, Tmat, (MANUEL) and also Tx */
  double y[4],dy[4];
  double elH,elHe,elTb,elTx,eldTb,eldTx;
//MANUEL

  /* other recfast variables */
  double OmegaB,zinitial,x_He0,x0;
  double x_H0=0.;
  double z,mu_H,Lalpha,Lalpha_He,DeltaB,DeltaB_He;
  double zstart,zend,rhs;
  int i,Nz;

//MANUEL
  // yet other new variables
  double V_rms2,dV2;
  double OmegaI,OmegaG,OmegaR;
  double z_rec,Tb_rec,Tx_rec;

  double tau,tstart,tend;
  int last_index_back;
  int index_thv;
//MANUEL


  /* introduced by JL for smoothing the various steps */
  double x0_previous,x0_new,s,weight;

  /* contains all quantities relevant for the integration algorithm */
  struct generic_integrator_workspace gi;

  /* contains all fixed parameters which should be passed to thermodynamics_derivs_with_recfast */
  struct thermodynamics_parameters_and_workspace tpaw;

//MANUEL
  // the stiff integrator for the thermodynamics module.
  extern int thermo_stiff_ndf15();
//MANUEL

  /** - allocate memory for thermodynamics interpolation tables (size known in advance) */
  preco->rt_size = ppr->recfast_Nz0;
  class_alloc(preco->recombination_table,preco->re_size*preco->rt_size*sizeof(double),pth->error_message);

//MANUEL
  // - allocate memory for vector of redshifts and the boolean variables stating whether some quantities are printed
  if (pth->compute_dm_thermo == _TRUE_) {
    class_alloc(pdmde->tau_vec,preco->rt_size*sizeof(double),pth->error_message);

    class_alloc(pthv,sizeof(struct thermo_vector),pth->error_message);

    //define the indices for pthv
    index_thv = 0;
    class_define_index(pthv->index_thv_H,_TRUE_,index_thv,1);
    class_define_index(pthv->index_thv_He,_TRUE_,index_thv,1);
    class_define_index(pthv->index_thv_Tb,_TRUE_,index_thv,1);
    class_define_index(pthv->index_thv_Tx,_TRUE_,index_thv,1);

    pthv->thv_size = index_thv;// = 4; clearly!!!!

    //allocate vectors for storing the values of all these quantities and their redshift-derivatives at a given redshift.
    class_calloc(pthv->y,pthv->thv_size,sizeof(double),pth->error_message);
    class_alloc(pthv->dy,pthv->thv_size*sizeof(double),pth->error_message);
  }
//MANUEL

  /** - initialize generic integrator with initialize_generic_integrator() */
  // class_call(initialize_generic_integrator(_RECFAST_INTEG_SIZE_, &gi),
  //            gi.error_message,
  //            pth->error_message);//MANUEL: ORIGINAL

//MANUEL
  class_call(initialize_generic_integrator(4, &gi),
             gi.error_message,
             pth->error_message);
//MANUEL
  /** - read a few precision/cosmological parameters */

  /* Nz */
  Nz=ppr->recfast_Nz0;

  /* preco->H0 is H0 in inverse seconds (while pba->H0 is [H0/c] in inverse Mpcs) */
  preco->H0 = pba->H0 * _c_ / _Mpc_over_m_;

  /* Omega_b */
  OmegaB = pba->Omega0_b;

//MANUEL
  /* some other background quantities */
  OmegaI = pba->Omega0_chi;
  OmegaG = pba->Omega0_g;
  OmegaR = OmegaG*(1. + 7./8.*3.046*pow(4./11.,4./3.));
//MANUEL

  /* Yp */
  preco->YHe = pth->YHe;

  /* Tnow */
  preco->Tnow = pba->T_cmb;

  /* z_initial */
  zinitial=ppr->recfast_z_initial;

  /* H_frac */
  preco->H_frac = ppr->recfast_H_frac;

  /* H fudging */
  class_test((ppr->recfast_Hswitch != _TRUE_) && (ppr->recfast_Hswitch != _FALSE_),
             pth->error_message,
             "RECFAST error: unknown H fudging scheme");
  preco->fu = ppr->recfast_fudge_H;
  if (ppr->recfast_Hswitch == _TRUE_)
    preco->fu += ppr->recfast_delta_fudge_H;

  /* He fudging */
  class_test((ppr->recfast_Heswitch < 0) || (ppr->recfast_Heswitch > 6),
             pth->error_message,
             "RECFAST error: unknown He fudging scheme");

  /* related quantities */
  z=zinitial;
  mu_H = 1./(1.-preco->YHe);
  //mu_T = _not4_ /(_not4_ - (_not4_-1.)*preco->YHe); /* recfast 1.4*/
  preco->fHe = preco->YHe/(_not4_ *(1.-preco->YHe)); /* recfast 1.4 */
  preco->Nnow = 3.*preco->H0*preco->H0*OmegaB/(8.*_PI_*_G_*mu_H*_m_H_);
  pth->n_e = preco->Nnow;

//MANUEL
  /* Other quantities and prefactors */
  if (pth->compute_dm_thermo == _TRUE_) {
    z_rec = pdmde->z_vec[0];
    Tb_rec = pdmde->dmdecoupling_table[0*pdmde->dm_size+pdmde->index_dm_Tb];
    Tx_rec = pdmde->dmdecoupling_table[0*pdmde->dm_size+pdmde->index_dm_Tx];

    if (pth->thermodynamics_verbose > 1) {
      fprintf(stdout," -> ChiDM RECFAST module (IDM-b)\n    At z = %.4e, we have Tb = %.4e K and Tx = %.4e K.\n",z_rec,Tb_rec,Tx_rec);

      if (pth->thermodynamics_verbose > 3)
        fprintf(stdout,"    zinitial = %.4e; compare to last DM dec. z = %.4e. They ought to be equal.\n",zinitial,z_rec);

      if (pth->beta_Tg == _TRUE_)
        fprintf(stdout,"    (beta_Tg = TRUE): You have chosen to evaluate the effective photoionization coefficient with the photon temperature instead of the baryon temperature. According to some literature, this seems to be the correct way to do things (see arXiv:1808.00001 footnote 11; and arXiv:1503.04827.).\n********************************************\n");
      if (pth->beta_Tg == _FALSE_)
        fprintf(stdout,"    (beta_Tg = FALSE): You have chosen to evaluate the effective photoionization coefficient with the baryon temperature instead of the photon temperature. According to some literature, this seems to be incorrect (see arXiv:1808.00001 footnote 11; and arXiv:1503.04827.).\n********************************************\n");
    }
  }
//MANUEL
  /* energy injection parameters */
  preco->annihilation = pth->annihilation;
  preco->has_on_the_spot = pth->has_on_the_spot;
  preco->annihilation_variation = pth->annihilation_variation;
  preco->annihilation_z = pth->annihilation_z;
  preco->annihilation_zmax = pth->annihilation_zmax;
  preco->annihilation_zmin = pth->annihilation_zmin;
  preco->decay = pth->decay;
  preco->annihilation_f_halo = pth->annihilation_f_halo;
  preco->annihilation_z_halo = pth->annihilation_z_halo;

  /* quantities related to constants defined in thermodynamics.h */
  //n = preco->Nnow * pow((1.+z),3);
  Lalpha = 1./_L_H_alpha_;
  Lalpha_He = 1./_L_He_2p_;
  DeltaB = _h_P_*_c_*(_L_H_ion_-_L_H_alpha_);
  preco->CDB = DeltaB/_k_B_;
  DeltaB_He = _h_P_*_c_*(_L_He1_ion_-_L_He_2s_);
  preco->CDB_He = DeltaB_He/_k_B_;
  preco->CB1 = _h_P_*_c_*_L_H_ion_/_k_B_;
  preco->CB1_He1 = _h_P_*_c_*_L_He1_ion_/_k_B_;
  preco->CB1_He2 = _h_P_*_c_*_L_He2_ion_/_k_B_;
  preco->CR = 2.*_PI_*(_m_e_/_h_P_)*(_k_B_/_h_P_);
  preco->CK = pow(Lalpha,3)/(8.*_PI_);
  preco->CK_He = pow(Lalpha_He,3)/(8.*_PI_);
  preco->CL = _c_*_h_P_/(_k_B_*Lalpha);
  preco->CL_He = _c_*_h_P_/(_k_B_/_L_He_2s_);
  preco->CT = (8./3.) * (_sigma_/(_m_e_*_c_)) *
    (8.*pow(_PI_,5)*pow(_k_B_,4)/ 15./ pow(_h_P_,3)/pow(_c_,3));

  preco->Bfact = _h_P_*_c_*(_L_He_2p_-_L_He_2s_)/_k_B_;

  /** - define the fields of the 'thermodynamics parameter and workspace' structure */
  tpaw.pba = pba;
  tpaw.ppr = ppr;
  tpaw.preco = preco;
  tpaw.pvecback = pvecback;
//MANUEL
  tpaw.pdmde = pdmde;
  tpaw.pthv = pthv;
//MANUEL

  /** - impose initial conditions at early times */

  class_test(zinitial < ppr->recfast_z_He_3,
             pth->error_message,
             "increase zinitial, otherwise should get initial conditions from recfast's get_init routine (less precise anyway)");

  y[0] = 1.;
  y[1] = 1.;
  x0 = 1.+2.*preco->fHe;
  // y[2] = preco->Tnow*(1.+z);//MANUEL: ORIGINAL
//MANUEL
  pdmde->temp_x = x0;
  if (pth->compute_dm_thermo == _FALSE_) {
    y[2] = preco->Tnow*(1.+z);
    y[3] = 0.;
  }
  else {
    y[2] = Tb_rec;
    y[3] = Tx_rec;
  }
//MANUEL

//MANUEL
  /** - filling tau_vec */
  if (pth->compute_dm_thermo == _TRUE_) {
    for(i=0; i<Nz; i++) {
      zend = zinitial * (double)(Nz-i-1) / (double)Nz;

      class_call(background_tau_of_z(pba,
                                     zend,
                                     &tend),
                 pba->error_message,
                 pth->error_message);

      *(pdmde->tau_vec+i) = tend;//filled from earliest to latest: smallest tau at i=0, to largest tau at i=Nz-1.

    }
  }
//MANUEL

  /** - loop over redshift steps Nz; integrate over each step with
      generic_integrator(), store the results in the table using
      thermodynamics_derivs_with_recfast()*/

//MANUEL: ORIGINAL: COMMENTED
  // for(i=0; i <Nz; i++) {
  //
  //   zstart = zinitial * (double)(Nz-i) / (double)Nz;
  //   zend   = zinitial * (double)(Nz-i-1) / (double)Nz;
  //
  //   z = zend;
  //
  //   /** - --> first approximation: H and Helium fully ionized */
  //
  //   if (z > ppr->recfast_z_He_1+ppr->recfast_delta_z_He_1) {
  //     x_H0 = 1.;
  //     x_He0 = 1.;
  //     x0 = 1.+2.*preco->fHe;
  //     y[0] = x_H0;
  //     y[1] = x_He0;
  //     y[2] = preco->Tnow*(1.+z);
  //   }
  //
  //   /** - --> second approximation: first Helium recombination (analytic approximation) */
  //
  //   else if (z > ppr->recfast_z_He_2+ppr->recfast_delta_z_He_2) {
  //     x_H0 = 1.;
  //     x_He0 = 1.;
  //
  //     rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;
  //
  //     /* smoothed transition */
  //     if (z > ppr->recfast_z_He_1-ppr->recfast_delta_z_He_1) {
  //       x0_previous = 1.+2.*preco->fHe;
  //       x0_new = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
  //
  //       /* get s from -1 to 1 */
  //       s = (ppr->recfast_z_He_1-z)/ppr->recfast_delta_z_He_1;
  //       /* infer f1(s) = smooth function interpolating from 0 to 1 */
  //       weight = f1(s);
  //
  //       x0 = weight*x0_new+(1.-weight)*x0_previous;
  //     }
  //     /* transition finished */
  //     else {
  //       x0 = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
  //     }
  //
  //     y[0] = x_H0;
  //     y[1] = x_He0;
  //     y[2] = preco->Tnow*(1.+z);
  //   }
  //
  //   /** - --> third approximation: first Helium recombination completed */
  //
  //   else if (z > ppr->recfast_z_He_3+ppr->recfast_delta_z_He_3) {
  //     x_H0 = 1.;
  //     x_He0 = 1.;
  //
  //     /* smoothed transition */
  //     if (z > ppr->recfast_z_He_2-ppr->recfast_delta_z_He_2) {
  //       rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;
  //       x0_previous = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
  //       x0_new = 1. + preco->fHe;
  //       /* get s from -1 to 1 */
  //       s = (ppr->recfast_z_He_2-z)/ppr->recfast_delta_z_He_2;
  //       /* infer f1(s) = smooth function interpolating from 0 to 1 */
  //       weight = f1(s);
  //
  //       x0 = weight*x0_new+(1.-weight)*x0_previous;
  //
  //     }
  //     /* transition finished */
  //     else {
  //       x0 = 1.+preco->fHe;
  //     }
  //
  //     y[0] = x_H0;
  //     y[1] = x_He0;
  //     y[2] = preco->Tnow*(1.+z);
  //   }
  //
  //   /** - --> fourth approximation: second Helium recombination starts (analytic approximation) */
  //
  //   else if (y[1] > ppr->recfast_x_He0_trigger) {
  //     x_H0 = 1.;
  //
  //     rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
  //     x_He0 = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));
  //
  //     /* smoothed transition */
  //     if (z > ppr->recfast_z_He_3-ppr->recfast_delta_z_He_3) {
  //       x0_previous = 1. + preco->fHe;
  //       x0_new = x_He0;
  //       /* get s from -1 to 1 */
  //       s = (ppr->recfast_z_He_3-z)/ppr->recfast_delta_z_He_3;
  //       /* infer f1(x) = smooth function interpolating from 0 to 1 */
  //       weight = f1(s);
  //
  //       x0 = weight*x0_new+(1.-weight)*x0_previous;
  //     }
  //     /* transition finished */
  //     else {
  //       x0 = x_He0;
  //     }
  //
  //     x_He0 = (x0-1.)/preco->fHe;
  //     y[0] = x_H0;
  //     y[1] = x_He0;
  //     y[2] = preco->Tnow*(1.+z);
  //   }
  //
  //   /** - --> fifth approximation: second Helium recombination (full
  //       evolution for Helium), H recombination starts (analytic
  //       approximation) */
  //
  //   else if (y[0] > ppr->recfast_x_H0_trigger) {
  //
  //     rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
  //     x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);
  //
  //     class_call(generic_integrator(thermodynamics_derivs_with_recfast,
  //                                   zstart,
  //                                   zend,
  //                                   y,
  //                                   &tpaw,
  //                                   ppr->tol_thermo_integration,
  //                                   ppr->smallest_allowed_variation,
  //                                   &gi),
  //                gi.error_message,
  //                pth->error_message);
  //
  //     y[0] = x_H0;
  //
  //     /* smoothed transition */
  //     if (ppr->recfast_x_He0_trigger - y[1] < ppr->recfast_x_He0_trigger_delta) {
  //       rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
  //       x0_previous = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));
  //       x0_new = y[0] + preco->fHe*y[1];
  //       /* get s from 0 to 1 */
  //       s = (ppr->recfast_x_He0_trigger - y[1])/ppr->recfast_x_He0_trigger_delta;
  //       /* infer f2(x) = smooth function interpolating from 0 to 1 */
  //       weight = f2(s);
  //
  //       x0 = weight*x0_new+(1.-weight)*x0_previous;
  //     }
  //     /* transition finished */
  //     else {
  //       x0 = y[0] + preco->fHe*y[1];
  //     }
  //
  //   }
  //
  //   /** - --> last case: full evolution for H and Helium */
  //
  //   else {
  //
  //     /* quantities used for smoothed transition */
  //     if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
  //       rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
  //       x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);
  //     }
  //
  //     class_call(generic_integrator(thermodynamics_derivs_with_recfast,
  //                                   zstart,
  //                                   zend,
  //                                   y,
  //                                   &tpaw,
  //                                   ppr->tol_thermo_integration,
  //                                   ppr->smallest_allowed_variation,
  //                                   &gi),
  //                gi.error_message,
  //                pth->error_message);
  //
  //     /* smoothed transition */
  //     if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
  //       /* get s from 0 to 1 */
  //       s = (ppr->recfast_x_H0_trigger - y[0])/ppr->recfast_x_H0_trigger_delta;
  //       /* infer f2(s) = smooth function interpolating from 0 to 1 */
  //       weight = f2(s);
  //
  //       x0 = weight*y[0]+(1.-weight)*x_H0 + preco->fHe*y[1];
  //
  //     }
  //     /* transition finished */
  //     else {
  //       x0 = y[0] + preco->fHe*y[1];
  //     }
  //   }
  //
  //   /** - --> store the results in the table */
  //   /* results are obtained in order of decreasing z, and stored in order of growing z */
  //
  //   /* redshift */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z)=zend;
  //
  //   /* ionization fraction */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe)=x0;
  //
  //   /* Tb */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb)=y[2];
  //
  //   /* get dTb/dz=dy[2] */
  //   class_call(thermodynamics_derivs_with_recfast(zend, y, dy, &tpaw,pth->error_message),
  //              pth->error_message,
  //              pth->error_message);
  //
  //   /* wb = (k_B/mu) Tb  = (k_B/mu) Tb */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
  //     = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * preco->YHe + x0 * (1.-preco->YHe)) * y[2];
  //
  //   /* cb2 = (k_B/mu) Tb (1-1/3 dlnTb/dlna) = (k_B/mu) Tb (1+1/3 (1+z) dlnTb/dz) */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2)
  //     = *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
  //     * (1. + (1.+zend) * dy[2] / y[2] / 3.);
  //
  //   /* dkappa/dtau = a n_e x_e sigma_T = a^{-2} n_e(today) x_e sigma_T (in units of 1/Mpc) */
  //   *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau)
  //     = (1.+zend) * (1.+zend) * preco->Nnow * x0 * _sigma_ * _Mpc_over_m_;
  //
  //   /* fprintf(stdout,"%e %e %e %e %e %e\n", */
  //   /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z), */
  //   /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe), */
  //   /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb), */
  //   /* 	    (1.+zend) * dy[2], */
  //   /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2), */
  //   /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau) */
  //   /* 	    ); */
  //
  // }
//MANUEL: ORIGINAL: COMMENTED

//MANUEL
  if (pth->compute_dm_thermo == _FALSE_) {

//MANUEL
    preco->beta_Tg = pth->beta_Tg;
//MANUEL

    for(i=0; i <Nz; i++) {

      zstart = zinitial * (double)(Nz-i) / (double)Nz;
      zend   = zinitial * (double)(Nz-i-1) / (double)Nz;

      z = zend;

      /** - --> first approximation: H and Helium fully ionized */

      if (z > ppr->recfast_z_He_1+ppr->recfast_delta_z_He_1) {
        x_H0 = 1.;
        x_He0 = 1.;
        x0 = 1.+2.*preco->fHe;
        y[0] = x_H0;
        y[1] = x_He0;
        y[2] = preco->Tnow*(1.+z);
  //MANUEL
        y[3] = 0.;
  //MANUEL
      }

      /** - --> second approximation: first Helium recombination (analytic approximation) */

      else if (z > ppr->recfast_z_He_2+ppr->recfast_delta_z_He_2) {
        x_H0 = 1.;
        x_He0 = 1.;

        rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;

        /* smoothed transition */
        if (z > ppr->recfast_z_He_1-ppr->recfast_delta_z_He_1) {
          x0_previous = 1.+2.*preco->fHe;
          x0_new = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));

          /* get s from -1 to 1 */
          s = (ppr->recfast_z_He_1-z)/ppr->recfast_delta_z_He_1;
          /* infer f1(s) = smooth function interpolating from 0 to 1 */
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        /* transition finished */
        else {
          x0 = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
        }

        y[0] = x_H0;
        y[1] = x_He0;
        y[2] = preco->Tnow*(1.+z);
  //MANUEL
        y[3] = 0.;
  //MANUEL
      }

      /** - --> third approximation: first Helium recombination completed */

      else if (z > ppr->recfast_z_He_3+ppr->recfast_delta_z_He_3) {
        x_H0 = 1.;
        x_He0 = 1.;

        /* smoothed transition */
        if (z > ppr->recfast_z_He_2-ppr->recfast_delta_z_He_2) {
          rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;
          x0_previous = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
          x0_new = 1. + preco->fHe;
          /* get s from -1 to 1 */
          s = (ppr->recfast_z_He_2-z)/ppr->recfast_delta_z_He_2;
          /* infer f1(s) = smooth function interpolating from 0 to 1 */
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;

        }
        /* transition finished */
        else {
          x0 = 1.+preco->fHe;
        }

        y[0] = x_H0;
        y[1] = x_He0;
        y[2] = preco->Tnow*(1.+z);
  //MANUEL
        y[3] = 0.;
  //MANUEL
      }

      /** - --> fourth approximation: second Helium recombination starts (analytic approximation) */

      else if (y[1] > ppr->recfast_x_He0_trigger) {
        x_H0 = 1.;

        rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
        x_He0 = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));

        /* smoothed transition */
        if (z > ppr->recfast_z_He_3-ppr->recfast_delta_z_He_3) {
          x0_previous = 1. + preco->fHe;
          x0_new = x_He0;
          /* get s from -1 to 1 */
          s = (ppr->recfast_z_He_3-z)/ppr->recfast_delta_z_He_3;
          /* infer f1(x) = smooth function interpolating from 0 to 1 */
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        /* transition finished */
        else {
          x0 = x_He0;
        }

        x_He0 = (x0-1.)/preco->fHe;
        y[0] = x_H0;
        y[1] = x_He0;
        y[2] = preco->Tnow*(1.+z);
  //MANUEL
        y[3] = 0.;
  //MANUEL
      }

      /** - --> fifth approximation: second Helium recombination (full
          evolution for Helium), H recombination starts (analytic
          approximation) */

      else if (y[0] > ppr->recfast_x_H0_trigger) {

        rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
        x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);

        class_call(generic_integrator(thermodynamics_derivs_with_recfast,
                                      zstart,
                                      zend,
                                      y,
                                      &tpaw,
                                      ppr->tol_thermo_integration,
                                      ppr->smallest_allowed_variation,
                                      &gi),
                   gi.error_message,
                   pth->error_message);

        y[0] = x_H0;

        /* smoothed transition */
        if (ppr->recfast_x_He0_trigger - y[1] < ppr->recfast_x_He0_trigger_delta) {
          rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
          x0_previous = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));
          x0_new = y[0] + preco->fHe*y[1];
          /* get s from 0 to 1 */
          s = (ppr->recfast_x_He0_trigger - y[1])/ppr->recfast_x_He0_trigger_delta;
          /* infer f2(x) = smooth function interpolating from 0 to 1 */
          weight = f2(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        /* transition finished */
        else {
          x0 = y[0] + preco->fHe*y[1];
        }

      }

      /** - --> last case: full evolution for H and Helium */

      else {

        /* quantities used for smoothed transition */
        if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
          rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
          x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);
        }

        class_call(generic_integrator(thermodynamics_derivs_with_recfast,
                                      zstart,
                                      zend,
                                      y,
                                      &tpaw,
                                      ppr->tol_thermo_integration,
                                      ppr->smallest_allowed_variation,
                                      &gi),
                   gi.error_message,
                   pth->error_message);

        /* smoothed transition */
        if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
          /* get s from 0 to 1 */
          s = (ppr->recfast_x_H0_trigger - y[0])/ppr->recfast_x_H0_trigger_delta;
          /* infer f2(s) = smooth function interpolating from 0 to 1 */
          weight = f2(s);

          x0 = weight*y[0]+(1.-weight)*x_H0 + preco->fHe*y[1];

        }
        /* transition finished */
        else {
          x0 = y[0] + preco->fHe*y[1];
        }
      }

      /** - --> store the results in the table */
      /* results are obtained in order of decreasing z, and stored in order of growing z */

      /* redshift */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z)=zend;

      /* ionization fraction */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe)=x0;

      /* Tb */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb)=y[2];

      /* get dTb/dz=dy[2] */
      class_call(thermodynamics_derivs_with_recfast(zend, y, dy, &tpaw,pth->error_message),
                 pth->error_message,
                 pth->error_message);

      /* wb = (k_B/mu) Tb  = (k_B/mu) Tb */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
        = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * preco->YHe + x0 * (1.-preco->YHe)) * y[2];

      /* cb2 = (k_B/mu) Tb (1-1/3 dlnTb/dlna) = (k_B/mu) Tb (1+1/3 (1+z) dlnTb/dz) */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2)
        = *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
        * (1. + (1.+zend) * dy[2] / y[2] / 3.);

      /* dkappa/dtau = a n_e x_e sigma_T = a^{-2} n_e(today) x_e sigma_T (in units of 1/Mpc) */
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau)
        = (1.+zend) * (1.+zend) * preco->Nnow * x0 * _sigma_ * _Mpc_over_m_;

      /* fprintf(stdout,"%e %e %e %e %e %e\n", */
      /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z), */
      /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe), */
      /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb), */
      /* 	    (1.+zend) * dy[2], */
      /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2), */
      /* 	    *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau) */
      /* 	    ); */

    }
  }
  else {

    preco->beta_Tg = pth->beta_Tg;

    for(i=0; i <Nz; i++) {

      zstart = zinitial * (double)(Nz-i) / (double)Nz;
      zend   = zinitial * (double)(Nz-i-1) / (double)Nz;

      z = zend;

      class_call(background_tau_of_z(pba,
                                     zstart,
                                     &tstart),
                 pba->error_message,
                 pth->error_message);

      class_call(background_tau_of_z(pba,
                                     zend,
                                     &tend),
                 pba->error_message,
                 pth->error_message);

      //1st approx: H and Helium fully ionized

      if (z > ppr->recfast_z_He_1+ppr->recfast_delta_z_He_1) {

        x_H0 = 1.;
        x_He0 = 1.;
        x0 = 1.+2.*preco->fHe;
        pdmde->temp_x = x0;
        preco->is_it_saha = _TRUE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = x_H0;
        elHe = x_He0;
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;
      }

      //2nd approx: first Helium recombination (analytic approximation)

      else if (z > ppr->recfast_z_He_2+ppr->recfast_delta_z_He_2) {

        x_H0 = 1.;
        x_He0 = 1.;

        rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;

        //smoothed transition
        if (z > ppr->recfast_z_He_1-ppr->recfast_delta_z_He_1) {
          x0_previous = 1.+2.*preco->fHe;
          x0_new = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));

          //get s from -1 to 1
          s = (ppr->recfast_z_He_1-z)/ppr->recfast_delta_z_He_1;
          //infer f1(s) = smooth function interpolating from 0 to 1
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        //transition finished
        else {
          x0 = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
        }

        pdmde->temp_x = x0;
        preco->is_it_saha = _TRUE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = x_H0;
        elHe = x_He0;
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;
      }

      //3rd approx: first Helium recombination completed

      else if (z > ppr->recfast_z_He_3+ppr->recfast_delta_z_He_3) {

        x_H0 = 1.;
        x_He0 = 1.;

        //smoothed transition
        if (z > ppr->recfast_z_He_2-ppr->recfast_delta_z_He_2) {
          rhs = exp( 1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He2/(preco->Tnow*(1.+z)) ) / preco->Nnow;
          x0_previous = 0.5*(sqrt(pow((rhs-1.-preco->fHe),2) + 4.*(1.+2.*preco->fHe)*rhs) - (rhs-1.-preco->fHe));
          x0_new = 1. + preco->fHe;
          //get s from -1 to 1
          s = (ppr->recfast_z_He_2-z)/ppr->recfast_delta_z_He_2;
          //infer f1(s) = smooth function interpolating from 0 to 1
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;

        }
        //transition finished
        else {
          x0 = 1.+preco->fHe;
        }

        pdmde->temp_x = x0;
        preco->is_it_saha = _TRUE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = x_H0;
        elHe = x_He0;
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;
      }

      //4th approx: second Helium recombination starts (analytic approximation)

      else if (y[1] > ppr->recfast_x_He0_trigger) {

        x_H0 = 1.;

        rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
        x_He0 = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));

        //smoothed transition
        if (z > ppr->recfast_z_He_3-ppr->recfast_delta_z_He_3) {
          x0_previous = 1. + preco->fHe;
          x0_new = x_He0;
          //get s from -1 to 1
          s = (ppr->recfast_z_He_3-z)/ppr->recfast_delta_z_He_3;
          //infer f1(x) = smooth function interpolating from 0 to 1
          weight = f1(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        //transition finished
        else {
          x0 = x_He0;
        }

        x_He0 = (x0-1.)/preco->fHe;
        pdmde->temp_x = x0;
        preco->is_it_saha = _TRUE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = x_H0;
        elHe = x_He0;
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;
      }

      //5th approx: second Helium recombination (full evolution for Helium), H recombination starts (analytic approximation)

      else if (y[0] > ppr->recfast_x_H0_trigger) {


        rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
        x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);

        preco->is_it_saha = _FALSE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = x_H0;
        elHe = pthv->y[pthv->index_thv_He];
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;

        //smoothed transition
        if (ppr->recfast_x_He0_trigger - y[1] < ppr->recfast_x_He0_trigger_delta) {
          rhs = 4.*exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1_He1/(preco->Tnow*(1.+z)))/preco->Nnow;
          x0_previous = 0.5*(sqrt(pow((rhs-1.),2) + 4.*(1.+preco->fHe)*rhs )- (rhs-1.));
          x0_new = y[0] + preco->fHe*y[1];
          //get s from 0 to 1
          s = (ppr->recfast_x_He0_trigger - y[1])/ppr->recfast_x_He0_trigger_delta;
          //infer f2(x) = smooth function interpolating from 0 to 1
          weight = f2(s);

          x0 = weight*x0_new+(1.-weight)*x0_previous;
        }
        //transition finished
        else {
          x0 = y[0] + preco->fHe*y[1];
        }

      }

      //last case: full evolution for H and Helium

      else {


        //quantities used for smoothed transition
        if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
          rhs = exp(1.5*log(preco->CR*preco->Tnow/(1.+z)) - preco->CB1/(preco->Tnow*(1.+z)))/preco->Nnow;
          x_H0 = 0.5*(sqrt(pow(rhs,2)+4.*rhs) - rhs);
        }

        preco->is_it_saha = _FALSE_;

        class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_recfast,
                                      tstart,
                                      tend,
                                      y,
                                      4,
                                      &tpaw,
                                      ppr->tol_dmde_integration,
                                      ppr->smallest_allowed_variation,
                                      pdmde->tau_vec,
                                      Nz,
                                      thermo_sources,
                                      pth->error_message),
                   pth->error_message,
                   pth->error_message);

        elH = pthv->y[pthv->index_thv_H];
        elHe = pthv->y[pthv->index_thv_He];
        elTb = pthv->y[pthv->index_thv_Tb];
        elTx = pthv->y[pthv->index_thv_Tx];

        y[0] = elH;
        y[1] = elHe;
        y[2] = elTb;
        y[3] = elTx;

        //smoothed transition
        if (ppr->recfast_x_H0_trigger - y[0] < ppr->recfast_x_H0_trigger_delta) {
          //get s from 0 to 1
          s = (ppr->recfast_x_H0_trigger - y[0])/ppr->recfast_x_H0_trigger_delta;
          //infer f2(s) = smooth function interpolating from 0 to 1
          weight = f2(s);

          x0 = weight*y[0]+(1.-weight)*x_H0 + preco->fHe*y[1];

        }
        //transition finished
        else {
          x0 = y[0] + preco->fHe*y[1];
        }
      }

      //store the results in the table
      //results are obtained in order of decreasing z, and stored in order of growing z

      //redshift
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_z)=zend;
      //ionization fraction
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_xe)=x0;
      //Tb
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tb)=elTb;
      //Tx
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Tx)=elTx;
      //get dTb/dz=dy[2]

      class_call(thermodynamics_tau_derivs_recfast(tend, y, dy, &tpaw,pth->error_message),
                 pth->error_message,
                 pth->error_message);

      eldTb = dy[2];
      eldTx = dy[3];

      class_call(background_tau_of_z(pba,
                                     zend,
                                     &tau),
                 pba->error_message,
                 pth->error_message);

      class_call(background_at_tau(pba,
                                   tau,
                                   pba->short_info,
                                   pba->inter_normal,
                                   &last_index_back,
                                   pvecback),
                 pba->error_message,
                 pth->error_message);

      eldTb *= (-1./pvecback[pba->index_bg_H]);
      eldTx *= (-1./pvecback[pba->index_bg_H]);

      //Vrms2
      if (pba->has_Vrms2 == _TRUE_) {
        if ((1./(1.+z)) <= 1.e-3) {
          V_rms2 = pba->Vrms2;
          dV2 = 0.;
        }
        else {
          V_rms2 = pba->Vrms2 * pow(1.e-3*(1.+z),2.);
          dV2 = 2.*V_rms2/(1.+z);
        }
      }

      // wb = (k_B/mu) Tb  = (k_B/mu) Tb
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_wb)
        = _k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * preco->YHe + x0 * (1.-preco->YHe)) * elTb;

      // cb2 = (k_B/mu) Tb (1-1/3 dlnTb/dlna) = (k_B/mu) Tb (1+1/3 (1+z) dlnTb/dz)
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cb2)
        = preco->recombination_table[(Nz-i-1)*preco->re_size+preco->index_re_wb]
        * (1. + (1.+zend) * eldTb / elTb / 3.);

      //dkappa/dtau = a n_e x_e sigma_T = a^{-2} n_e(today) x_e sigma_T (in units of 1/Mpc)
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dkappadtau)
        = (1.+zend) * (1.+zend) * preco->Nnow * x0 * _sigma_ * _Mpc_over_m_;

      //cx2
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_cx2) = pdmde->pref2 * pdmde->pref3 * (elTx + pth->pref_cx2*(1.+zend) * eldTx / 3.);

      //Rx
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Rx) = (1.+zend)*(1.+zend) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(elTb + pdmde->pref3*elTx), (pth->pow_n + 1.)/2.);

      //Rxprime
      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_Rxprime) = pth->mxb * preco->recombination_table[(Nz-i-1)*preco->re_size+preco->index_re_Rx];

      *(preco->recombination_table+(Nz-i-1)*preco->re_size+preco->index_re_dRx) = -pvecback[pba->index_bg_H]*(2./(1.+z) + ((pth->pow_n + 1.)/2.)*(dV2/3. + pdmde->pref2*(eldTb + pdmde->pref3*eldTx))/(V_rms2/3. + pdmde->pref2*(elTb + pdmde->pref3*elTx)))*preco->recombination_table[(Nz-i-1)*preco->re_size+preco->index_re_Rx];
    }
  }

//MANUEL

  /** - cleanup generic integrator with cleanup_generic_integrator() */

  class_call(cleanup_generic_integrator(&gi),
             gi.error_message,
             pth->error_message);

//MANUEL
  if (pth->compute_dm_thermo == _TRUE_) {
    class_call(thermo_vector_free(pthv),
               pth->error_message,
               pth->error_message);

    free(pdmde->tau_vec);
  }
//MANUEL

  return _SUCCESS_;
}

/**
 * Subroutine evaluating the derivative with respect to redshift of
 * thermodynamical quantities (from RECFAST version 1.4).
 *
 * Computes derivatives of the three variables to integrate: \f$ d x_H
 * / dz, d x_{He} / dz, d T_{mat} / dz \f$.
 *
 * This is one of the few functions in the code which are passed to
 * the generic_integrator() routine.  Since generic_integrator()
 * should work with functions passed from various modules, the format
 * of the arguments is a bit special:
 *
 * - fixed parameters and workspaces are passed through a generic
 *   pointer. Here, this pointer contains the precision, background
 *   and recombination structures, plus a background vector, but
 *   generic_integrator() doesn't know its fine structure.
 *
 * - the error management is a bit special: errors are not written as
 *   usual to pth->error_message, but to a generic error_message
 *   passed in the list of arguments.
 *
 * @param z                        Input: redshift
 * @param y                        Input: vector of variable to integrate
 * @param dy                       Output: its derivative (already allocated)
 * @param parameters_and_workspace Input: pointer to fixed parameters (e.g. indices) and workspace (already allocated)
 * @param error_message            Output: error message
 */

int thermodynamics_derivs_with_recfast(
                                       double z,
                                       double * y,
                                       double * dy,
                                       void * parameters_and_workspace,
                                       ErrorMsg error_message
                                       ) {


  /* define local variables */

  double x,n,n_He,Trad,Tmat,x_H,x_He,Hz,dHdz,epsilon;
  double Rup,Rdown,K,K_He,Rup_He,Rdown_He,He_Boltz;
  double timeTh,timeH;
  double sq_0,sq_1;

  /* new in recfast 1.4: */
  double Rdown_trip,Rup_trip,tauHe_s,pHe_s,Doppler,gamma_2Ps,pb,qb,AHcon;
  double tauHe_t,pHe_t,CL_PSt,gamma_2Pt;
  double CfHe_t=0.;
  int Heflag;

//MANUEL
  double Rup_Tg,Rup_He_Tg,Rup_trip_Tg;//for photoionization with Tg instead of Tb
  double beta,beta_He,beta_trip;//for photoionization with Tg instead of Tb
//MANUEL

  struct thermodynamics_parameters_and_workspace * ptpaw;
  struct precision * ppr;
  struct background * pba;
  struct recombination * preco;
  double * pvecback;

  /* used for energy injection from dark matter */
  double C;
  //double C_He;
  double energy_rate;

  double tau;
  double chi_heat;
  double chi_ion_H;
  int last_index_back;

  ptpaw = parameters_and_workspace;
  ppr = ptpaw->ppr;
  pba = ptpaw->pba;
  preco = ptpaw->preco;
  pvecback = ptpaw->pvecback;

  x_H = y[0];
  x_He = y[1];
  x = x_H + preco->fHe * x_He;
  Tmat = y[2];

  n = preco->Nnow * (1.+z) * (1.+z) * (1.+z);
  n_He = preco->fHe * n;
  Trad = preco->Tnow * (1.+z);

  class_call(background_tau_of_z(pba,
                                 z,
                                 &tau),
             pba->error_message,
             error_message);

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  class_call(thermodynamics_energy_injection(ppr,pba,preco,z,&energy_rate,error_message),
             error_message,
             error_message);

  /* Hz is H in inverse seconds (while pvecback returns [H0/c] in inverse Mpcs) */
  Hz=pvecback[pba->index_bg_H]* _c_ / _Mpc_over_m_;

  Rdown=1.e-19*_a_PPB_*pow((Tmat/1.e4),_b_PPB_)/(1.+_c_PPB_*pow((Tmat/1.e4),_d_PPB_));
  Rup = Rdown * pow((preco->CR*Tmat),1.5)*exp(-preco->CDB/Tmat);

  sq_0 = sqrt(Tmat/_T_0_);
  sq_1 = sqrt(Tmat/_T_1_);
  Rdown_He = _a_VF_/(sq_0 * pow((1.+sq_0),(1.-_b_VF_)) * pow((1. + sq_1),(1. + _b_VF_)));
  Rup_He = 4.*Rdown_He*pow((preco->CR*Tmat),1.5)*exp(-preco->CDB_He/Tmat);
  K = preco->CK/Hz;

  /* following is from recfast 1.5 */

  if (ppr->recfast_Hswitch == _TRUE_ )
    K *= 1.
      + ppr->recfast_AGauss1*exp(-pow((log(1.+z)-ppr->recfast_zGauss1)/ppr->recfast_wGauss1,2))
      + ppr->recfast_AGauss2*exp(-pow((log(1.+z)-ppr->recfast_zGauss2)/ppr->recfast_wGauss2,2));

  /* end of new recfast 1.5 piece */

  /* following is from recfast 1.4 */

  Rdown_trip = _a_trip_/(sq_0*pow((1.+sq_0),(1.-_b_trip_)) * pow((1.+sq_1),(1.+_b_trip_)));
  Rup_trip = Rdown_trip*exp(-_h_P_*_c_*_L_He2St_ion_/(_k_B_*Tmat))*pow(preco->CR*Tmat,1.5)*4./3.;

//MANUEL
  if (preco->beta_Tg == _TRUE_) {//MANUEL: NOTE: photoionization rates with Trad instead of Tmat

    Rup_Tg = (1.e-19*_a_PPB_*pow((Trad/1.e4),_b_PPB_)/(1.+_c_PPB_*pow((Trad/1.e4),_d_PPB_))) * pow((preco->CR*Trad),1.5)*exp(-preco->CDB/Trad);

    Rup_He_Tg = 4.*( _a_VF_/(sqrt(Trad/_T_0_) * pow((1.+sqrt(Trad/_T_0_)),(1.-_b_VF_)) * pow((1. + sqrt(Trad/_T_1_)),(1. + _b_VF_))) )*pow((preco->CR*Trad),1.5)*exp(-preco->CDB_He/Trad);

    Rup_trip_Tg = ( _a_trip_/(sqrt(Trad/_T_0_)*pow((1.+sqrt(Trad/_T_0_)),(1.-_b_trip_)) * pow((1.+sqrt(Trad/_T_1_)),(1.+_b_trip_))) )*exp(-_h_P_*_c_*_L_He2St_ion_/(_k_B_*Trad))*pow(preco->CR*Trad,1.5)*4./3.;

    Rup = Rup_Tg;
    Rup_He = Rup_He_Tg;
    Rup_trip = Rup_trip_Tg;
  }
//MANUEL

  if ((x_He < 5.e-9) || (x_He > ppr->recfast_x_He0_trigger2))
    Heflag = 0;
  else
    Heflag = ppr->recfast_Heswitch;

  if (Heflag == 0)
    K_He = preco->CK_He/Hz;
  else {
    tauHe_s = _A2P_s_*preco->CK_He*3.*n_He*(1.-x_He)/Hz;
    pHe_s = (1.-exp(-tauHe_s))/tauHe_s;
    K_He = 1./(_A2P_s_*pHe_s*3.*n_He*(1.-x_He));

    /*    if (((Heflag == 2) || (Heflag >= 5)) && (x_H < 0.99999)) { */
    if (((Heflag == 2) || (Heflag >= 5)) && (x_H < 0.9999999)) { /* threshold changed by Antony Lewis in 2008 to get smoother Helium */

      Doppler = 2.*_k_B_*Tmat/(_m_H_*_not4_*_c_*_c_);
      Doppler = _c_*_L_He_2p_*sqrt(Doppler);
      gamma_2Ps = 3.*_A2P_s_*preco->fHe*(1.-x_He)*_c_*_c_
        /(sqrt(_PI_)*_sigma_He_2Ps_*8.*_PI_*Doppler*(1.-x_H))
        /pow(_c_*_L_He_2p_,2);
      pb = 0.36;
      qb = ppr->recfast_fudge_He;
      AHcon = _A2P_s_/(1.+pb*pow(gamma_2Ps,qb));
      K_He=1./((_A2P_s_*pHe_s+AHcon)*3.*n_He*(1.-x_He));
    }

    if (Heflag >= 3) {
      tauHe_t = _A2P_t_*n_He*(1.-x_He)*3./(8.*_PI_*Hz*pow(_L_He_2Pt_,3));
      pHe_t = (1. - exp(-tauHe_t))/tauHe_t;
      CL_PSt = _h_P_*_c_*(_L_He_2Pt_ - _L_He_2St_)/_k_B_;
      if ((Heflag == 3) || (Heflag == 5) || (x_H >= 0.99999)) {

//MANUEL
        if (preco->beta_Tg == _TRUE_)
          CfHe_t = _A2P_t_*pHe_t*exp(-CL_PSt/Trad);//MANUEL: NOTE: Trad instead of Tmat
        else
          CfHe_t = _A2P_t_*pHe_t*exp(-CL_PSt/Tmat);//MANUEL: ORIGINAL
//MANUEL

        CfHe_t = CfHe_t/(Rup_trip+CfHe_t);
      }
      else {
        Doppler = 2.*_k_B_*Tmat/(_m_H_*_not4_*_c_*_c_);
        Doppler = _c_*_L_He_2Pt_*sqrt(Doppler);
        gamma_2Pt = 3.*_A2P_t_*preco->fHe*(1.-x_He)*_c_*_c_
          /(sqrt(_PI_)*_sigma_He_2Pt_*8.*_PI_*Doppler*(1.-x_H))
          /pow(_c_*_L_He_2Pt_,2);
        pb = 0.66;
        qb = 0.9;
        AHcon = _A2P_t_/(1.+pb*pow(gamma_2Pt,qb))/3.;

//MANUEL
        if (preco->beta_Tg == _TRUE_)
          CfHe_t = (_A2P_t_*pHe_t+AHcon)*exp(-CL_PSt/Trad);//MANUEL: NOTE: Trad instead of Tmat
        else
          CfHe_t = (_A2P_t_*pHe_t+AHcon)*exp(-CL_PSt/Tmat);//MANUEL: ORIGINAL
//MANUEL

        CfHe_t = CfHe_t/(Rup_trip+CfHe_t);
      }
    }
  }

  /* end of new recfast 1.4 piece */

  timeTh=(1./(preco->CT*pow(Trad,4)))*(1.+x+preco->fHe)/x;
  timeH=2./(3.*preco->H0*pow(1.+z,1.5));

  /************/
  /* hydrogen */
  /************/

  if (x_H > ppr->recfast_x_H0_trigger)
    dy[0] = 0.;
  else {

    /* Peebles' coefficient (approximated as one when the Hydrogen
       ionization fraction is very close to one) */
    if (x_H < ppr->recfast_x_H0_trigger2) {
      C = (1. + K*_Lambda_*n*(1.-x_H))/(1./preco->fu+K*_Lambda_*n*(1.-x_H)/preco->fu +K*Rup*n*(1.-x_H));
    }
    else {
      C = 1.;
    }

    /* For DM annihilation: fraction of injected energy going into
       ionization and Lya excitation */

    /* - old approximation from Chen and Kamionkowski: */

    //chi_ion_H = (1.-x)/3.;

    /* coefficient as revised by Slatyer et al. 2013 (in fact it is a fit by Vivian Poulin of columns 1 and 2 in Table V of Slatyer et al. 2013): */

    if (x < 1.)
      chi_ion_H = 0.369202*pow(1.-pow(x,0.463929),1.70237);
    else
      chi_ion_H = 0.;

    /* evolution of hydrogen ionisation fraction: */

    // JL: test for debugginf reio_inter
    //fprintf(stdout,"%e  %e  %e  %e\n",z,Tmat,K*_Lambda_*n,K*Rup*n);

//MANUEL
    if (preco->beta_Tg == _TRUE_)
      beta = Rup*exp(-preco->CL/Trad);//MANUEL: NOTE: Trad instead of Tmat
    else
      beta = Rup*exp(-preco->CL/Tmat);//MANUEL: ORIGINAL
//MANUEL

    dy[0] = (x*x_H*n*Rdown - beta*(1.-x_H)) * C / (Hz*(1.+z))       /* Peeble's equation with fudged factors */
      -energy_rate*chi_ion_H/n*(1./_L_H_ion_+(1.-C)/_L_H_alpha_)/(_h_P_*_c_*Hz*(1.+z)); /* energy injection (neglect fraction going to helium) */

  }

  /************/
  /* helium   */
  /************/

  if (x_He < 1.e-15)
    dy[1]=0.;
  else {

//MANUEL
    if (preco->beta_Tg == _TRUE_) {

      if (preco->Bfact/Trad < 680.)//MANUEL: CHECK: is this correct?
        He_Boltz=exp(preco->Bfact/Trad);
      else
        He_Boltz=exp(680.);

      beta_He = Rup_He*exp(-preco->CL_He/Trad);//MANUEL: NOTE: Trad instead of Tmat
      beta_trip = Rup_trip*exp(-_h_P_*_c_*_L_He_2St_/(_k_B_*Trad));//MANUEL: NOTE: Trad instead of Tmat
    }
    else {//MANUEL: ORIGINAL

      if (preco->Bfact/Tmat < 680.)
        He_Boltz=exp(preco->Bfact/Tmat);//MANUEL: ORIGINAL
      else
        He_Boltz=exp(680.);//MANUEL: ORIGINAL

      beta_He = Rup_He*exp(-preco->CL_He/Tmat);//MANUEL: ORIGINAL
      beta_trip = Rup_trip*exp(-_h_P_*_c_*_L_He_2St_/(_k_B_*Tmat));//MANUEL: ORIGINAL
    }
//MANUEL

    /* equations modified to take into account energy injection from dark matter */
    //C_He=(1. + K_He*_Lambda_He_*n_He*(1.-x_He)*He_Boltz)/(1. + K_He*(_Lambda_He_+Rup_He)*n_He*(1.-x_He)*He_Boltz);

    dy[1] = ((x*x_He*n*Rdown_He - beta_He*(1.-x_He))
             *(1. + K_He*_Lambda_He_*n_He*(1.-x_He)*He_Boltz))
      /(Hz*(1+z)* (1. + K_He*(_Lambda_He_+Rup_He)*n_He*(1.-x_He)*He_Boltz)); /* in case of energy injection due to DM, we neglect the contribution to helium ionization */

    /* following is from recfast 1.4 */
    /* this correction is not self-consistent when there is energy injection  from dark matter, and leads to nan's  at small redshift (unimportant when reionization takes over before that redshift) */

    if (Heflag >= 3)
      dy[1] = dy[1] +
        (x*x_He*n*Rdown_trip
         - (1.-x_He)*3.*beta_trip)
        *CfHe_t/(Hz*(1.+z));

    /* end of new recfast 1.4 piece */

  }

  if (timeTh < preco->H_frac*timeH) {
    /*   dy[2]=Tmat/(1.+z); */
    /* v 1.5: like in camb, add here a smoothing term as suggested by Adam Moss */
    dHdz=-pvecback[pba->index_bg_H_prime]/pvecback[pba->index_bg_H]/pba->a_today* _c_ / _Mpc_over_m_;
    epsilon = Hz * (1.+x+preco->fHe) / (preco->CT*pow(Trad,3)*x);
    dy[2] = preco->Tnow + epsilon*((1.+preco->fHe)/(1.+preco->fHe+x))*((dy[0]+preco->fHe*dy[1])/x)
      - epsilon* dHdz/Hz + 3.*epsilon/(1.+z);
  }
  else {
    /* equations modified to take into account energy injection from dark matter */

    //chi_heat = (1.+2.*preio->reionization_table[i*preio->re_size+preio->index_re_xe])/3.; // old approximation from Chen and Kamionkowski

    // coefficient as revised by Slatyer et al. 2013 (in fact it is a fit by Vivian Poulin of columns 1 and 2 in Table V of Slatyer et al. 2013)
    if (x < 1.)
      chi_heat = MIN(0.996857*(1.-pow(1.-pow(x,0.300134),1.51035)),1);
    else
      chi_heat = 1.;

    dy[2]= preco->CT * pow(Trad,4) * x / (1.+x+preco->fHe) * (Tmat-Trad) / (Hz*(1.+z)) + 2.*Tmat/(1.+z)
      -2./(3.*_k_B_)*energy_rate*chi_heat/n/(1.+preco->fHe+x)/(Hz*(1.+z)); /* energy injection */
  }

//MANUEL
  dy[3] = 0.;
//MANUEL

  return _SUCCESS_;
}

//MANUEL
/**
 * Subroutine evaluating the derivative with respect to conformal time of
 * thermodynamical quantities, for the RECFAST recombination routine.
 *
 * Computes derivatives of the three variables to integrate: dTb/dtau, dTx/dtau.
 *
 * This is one of the few functions in the code which are passed to
 * the stiff integrator thermo_stiff_ndf15() routine.
 *
 * @param tau                      Input: conformal time
 * @param y                        Input: vector of variable to integrate
 * @param dy                       Output: its derivative (already allocated)
 * @param parameters_and_workspace Input: pointer to fixed parameters (e.g. indices) and workspace (already allocated)
 * @param error_message            Output: error message
 */

int thermodynamics_tau_derivs_recfast(
                                       double tau,
                                       double * y,
                                       double * dy,
                                       void * parameters_and_workspace,
                                       ErrorMsg error_message
                                       ) {


  /* define local variables */

  double x,n,n_He,Trad,Tmat,x_H,x_He,Hz;
  double Rup,Rdown,K,K_He,Rup_He,Rdown_He,He_Boltz;
  double sq_0,sq_1;
  double Tex;

  /* new in recfast 1.4: */
  double Rdown_trip,Rup_trip,tauHe_s,pHe_s,Doppler,gamma_2Ps,pb,qb,AHcon;
  double tauHe_t,pHe_t,CL_PSt,gamma_2Pt;
  double CfHe_t=0.;
  int Heflag;

  struct thermodynamics_parameters_and_workspace * ptpaw;
  struct precision * ppr;
  struct background * pba;
  struct recombination * preco;
  double * pvecback;
  struct dmdecoupling * pdmde;

  /* used for energy injection from dark matter */
  double C;
  //double C_He;
  double energy_rate;

  double z;
  double chi_heat;
  double chi_ion_H;
  int last_index_back;

  /* some more declarations */
  double S,mtot;
  double V_rms2,dV2;
  double aH,mub,dkdt,Rg,Rx,Rxprime;

  double Rup_Tg,Rup_He_Tg,Rup_trip_Tg;//for photoionization with Tg instead of Tb
  double beta,beta_He,beta_trip;//for photoionization with Tg instead of Tb

  ptpaw = parameters_and_workspace;
  ppr = ptpaw->ppr;
  pba = ptpaw->pba;
  preco = ptpaw->preco;
  pvecback = ptpaw->pvecback;
  pdmde = ptpaw->pdmde;

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  z = (1./pvecback[pba->index_bg_a]) - 1.;
  aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

  S = pba->Omega0_chi/pba->Omega0_b;
  mtot = (pdmde->dmx*_MeV_over_kg_)/pdmde->mxb;// [kg] total DM+b mass

  Trad = preco->Tnow * (1.+z);

  /* Vrms2 */
  if (pba->has_Vrms2 == _TRUE_) {
    if ((1./(1.+z)) <= 1.e-3) {
      V_rms2 = pba->Vrms2;
      dV2 = 0.;
    }
    else {
      V_rms2 = pba->Vrms2 * pow(1.e-3*(1.+z),2.);
      dV2 = pba->Vrms2*1.e-6*2.*(1.+z);
    }
  }

  if (preco->is_it_saha == _TRUE_) {
    Tmat = y[2];
    Tex = y[3];

    mub = _m_H_*(1. + _not4_*preco->fHe)/(1. + preco->fHe + pdmde->temp_x);

    dkdt = (1.+z) * (1.+z) * preco->Nnow * pdmde->temp_x * _sigma_ * _Mpc_over_m_;
    Rg = (4./3.)*((1.+z)*pba->Omega0_g/pba->Omega0_b)*dkdt;

    Rx = (1.+z) * (1.+z) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(Tmat + pdmde->pref3*Tex), (pdmde->dpn + 1.)/2.);
    Rxprime = pdmde->mxb*Rx;

    dy[2] = -2*aH*Tmat + 2.*(mub/_m_e_)*Rg*(Trad-Tmat);

    if (pdmde->is_Tb_blind == _FALSE_)
      dy[2] += 2.*S*(mub/mtot)*Rx*(Tex-Tmat);

    dy[3] = -2.*aH*Tex + 2.*Rxprime*(Tmat-Tex);
  }

  else {
    x_H = y[0];
    x_He = y[1];
    x = x_H + preco->fHe * x_He;
    Tmat = y[2];
    Tex = y[3];

    n = preco->Nnow * (1.+z) * (1.+z) * (1.+z);
    n_He = preco->fHe * n;

    class_call(thermodynamics_energy_injection(ppr,pba,preco,z,&energy_rate,error_message),
               error_message,
               error_message);

    /* Hz is H in inverse seconds (while pvecback returns [H0/c] in inverse Mpcs) */
    Hz=pvecback[pba->index_bg_H]* _c_ / _Mpc_over_m_;

    Rdown=1.e-19*_a_PPB_*pow((Tmat/1.e4),_b_PPB_)/(1.+_c_PPB_*pow((Tmat/1.e4),_d_PPB_));
    Rup = Rdown * pow((preco->CR*Tmat),1.5)*exp(-preco->CDB/Tmat);

    sq_0 = sqrt(Tmat/_T_0_);
    sq_1 = sqrt(Tmat/_T_1_);
    Rdown_He = _a_VF_/(sq_0 * pow((1.+sq_0),(1.-_b_VF_)) * pow((1. + sq_1),(1. + _b_VF_)));
    Rup_He = 4.*Rdown_He*pow((preco->CR*Tmat),1.5)*exp(-preco->CDB_He/Tmat);
    K = preco->CK/Hz;

    /* following is from recfast 1.5 */

    if (ppr->recfast_Hswitch == _TRUE_ )
      K *= 1.
        + ppr->recfast_AGauss1*exp(-pow((log(1.+z)-ppr->recfast_zGauss1)/ppr->recfast_wGauss1,2))
        + ppr->recfast_AGauss2*exp(-pow((log(1.+z)-ppr->recfast_zGauss2)/ppr->recfast_wGauss2,2));

    /* end of new recfast 1.5 piece */

    /* following is from recfast 1.4 */

    Rdown_trip = _a_trip_/(sq_0*pow((1.+sq_0),(1.-_b_trip_)) * pow((1.+sq_1),(1.+_b_trip_)));
    Rup_trip = Rdown_trip*exp(-_h_P_*_c_*_L_He2St_ion_/(_k_B_*Tmat))*pow(preco->CR*Tmat,1.5)*4./3.;

    if (preco->beta_Tg == _TRUE_) {//MANUEL: NOTE: photoionization rates with Trad instead of Tmat

      Rup_Tg = (1.e-19*_a_PPB_*pow((Trad/1.e4),_b_PPB_)/(1.+_c_PPB_*pow((Trad/1.e4),_d_PPB_))) * pow((preco->CR*Trad),1.5)*exp(-preco->CDB/Trad);

      Rup_He_Tg = 4.*( _a_VF_/(sqrt(Trad/_T_0_) * pow((1.+sqrt(Trad/_T_0_)),(1.-_b_VF_)) * pow((1. + sqrt(Trad/_T_1_)),(1. + _b_VF_))) )*pow((preco->CR*Trad),1.5)*exp(-preco->CDB_He/Trad);

      Rup_trip_Tg = ( _a_trip_/(sqrt(Trad/_T_0_)*pow((1.+sqrt(Trad/_T_0_)),(1.-_b_trip_)) * pow((1.+sqrt(Trad/_T_1_)),(1.+_b_trip_))) )*exp(-_h_P_*_c_*_L_He2St_ion_/(_k_B_*Trad))*pow(preco->CR*Trad,1.5)*4./3.;

      Rup = Rup_Tg;
      Rup_He = Rup_He_Tg;
      Rup_trip = Rup_trip_Tg;
    }

    if ((x_He < 5.e-9) || (x_He > ppr->recfast_x_He0_trigger2))
      Heflag = 0;
    else
      Heflag = ppr->recfast_Heswitch;

    if (Heflag == 0)
      K_He = preco->CK_He/Hz;
    else {
      tauHe_s = _A2P_s_*preco->CK_He*3.*n_He*(1.-x_He)/Hz;
      pHe_s = (1.-exp(-tauHe_s))/tauHe_s;
      K_He = 1./(_A2P_s_*pHe_s*3.*n_He*(1.-x_He));

      /*    if (((Heflag == 2) || (Heflag >= 5)) && (x_H < 0.99999)) { */
      if (((Heflag == 2) || (Heflag >= 5)) && (x_H < 0.9999999)) { /* threshold changed by Antony Lewis in 2008 to get smoother Helium */

        Doppler = 2.*_k_B_*Tmat/(_m_H_*_not4_*_c_*_c_);
        Doppler = _c_*_L_He_2p_*sqrt(Doppler);
        gamma_2Ps = 3.*_A2P_s_*preco->fHe*(1.-x_He)*_c_*_c_
          /(sqrt(_PI_)*_sigma_He_2Ps_*8.*_PI_*Doppler*(1.-x_H))
          /pow(_c_*_L_He_2p_,2);
        pb = 0.36;
        qb = ppr->recfast_fudge_He;
        AHcon = _A2P_s_/(1.+pb*pow(gamma_2Ps,qb));
        K_He=1./((_A2P_s_*pHe_s+AHcon)*3.*n_He*(1.-x_He));
      }

      if (Heflag >= 3) {
        tauHe_t = _A2P_t_*n_He*(1.-x_He)*3./(8.*_PI_*Hz*pow(_L_He_2Pt_,3));
        pHe_t = (1. - exp(-tauHe_t))/tauHe_t;
        CL_PSt = _h_P_*_c_*(_L_He_2Pt_ - _L_He_2St_)/_k_B_;
        if ((Heflag == 3) || (Heflag == 5) || (x_H >= 0.99999)) {

//MANUEL
        if (preco->beta_Tg == _TRUE_)
          CfHe_t = _A2P_t_*pHe_t*exp(-CL_PSt/Trad);//MANUEL: NOTE: Trad instead of Tmat
        else
          CfHe_t = _A2P_t_*pHe_t*exp(-CL_PSt/Tmat);//MANUEL: ORIGINAL
//MANUEL

          CfHe_t = CfHe_t/(Rup_trip+CfHe_t);
        }
        else {
          Doppler = 2.*_k_B_*Tmat/(_m_H_*_not4_*_c_*_c_);
          Doppler = _c_*_L_He_2Pt_*sqrt(Doppler);
          gamma_2Pt = 3.*_A2P_t_*preco->fHe*(1.-x_He)*_c_*_c_
            /(sqrt(_PI_)*_sigma_He_2Pt_*8.*_PI_*Doppler*(1.-x_H))
            /pow(_c_*_L_He_2Pt_,2);
          pb = 0.66;
          qb = 0.9;
          AHcon = _A2P_t_/(1.+pb*pow(gamma_2Pt,qb))/3.;

//MANUEL
        if (preco->beta_Tg == _TRUE_)
          CfHe_t = (_A2P_t_*pHe_t+AHcon)*exp(-CL_PSt/Trad);//MANUEL: NOTE: Trad instead of Tmat
        else
          CfHe_t = (_A2P_t_*pHe_t+AHcon)*exp(-CL_PSt/Tmat);//MANUEL: ORIGINAL
//MANUEL

          CfHe_t = CfHe_t/(Rup_trip+CfHe_t);
        }
      }
    }

    /* end of new recfast 1.4 piece */

    /* some more definitions */
    mub = _m_H_*(1. + _not4_*preco->fHe)/(1. + preco->fHe + x);

    /************/
    /* hydrogen */
    /************/

    if (x_H > ppr->recfast_x_H0_trigger)
      dy[0] = 0.;
    else {

      /* Peebles' coefficient (approximated as one when the Hydrogen
         ionization fraction is very close to one) */
      if (x_H < ppr->recfast_x_H0_trigger2) {
        C = (1. + K*_Lambda_*n*(1.-x_H))/(1./preco->fu+K*_Lambda_*n*(1.-x_H)/preco->fu +K*Rup*n*(1.-x_H));
      }
      else {
        C = 1.;
      }

      /* For DM annihilation: fraction of injected energy going into
         ionization and Lya excitation */

      /* - old approximation from Chen and Kamionkowski: */

      //chi_ion_H = (1.-x)/3.;

      /* coefficient as revised by Slatyer et al. 2013 (in fact it is a fit by Vivian Poulin of columns 1 and 2 in Table V of Slatyer et al. 2013): */

      if (x < 1.)
        chi_ion_H = 0.369202*pow(1.-pow(x,0.463929),1.70237);
      else
        chi_ion_H = 0.;

      /* evolution of hydrogen ionisation fraction: */

      // JL: test for debugginf reio_inter
      //fprintf(stdout,"%e  %e  %e  %e\n",z,Tmat,K*_Lambda_*n,K*Rup*n);

      if (preco->beta_Tg == _TRUE_)
        beta = Rup*exp(-preco->CL/Trad);//MANUEL: NOTE: Trad instead of Tmat
      else
        beta = Rup*exp(-preco->CL/Tmat);//MANUEL: ORIGINAL

      dy[0] = ((x*x_H*n*Rdown - beta*(1.-x_H)) * C / (Hz*(1.+z))       /* Peeble's equation with fudged factors */
        -energy_rate*chi_ion_H/n*(1./_L_H_ion_+(1.-C)/_L_H_alpha_)/(_h_P_*_c_*Hz*(1.+z)))*(-pvecback[pba->index_bg_H]); /* energy injection (neglect fraction going to helium) */

    }

    /************/
    /* helium   */
    /************/

    if (x_He < 1.e-15)
      dy[1]=0.;
    else {

//MANUEL
      if (preco->beta_Tg == _TRUE_) {

        if (preco->Bfact/Trad < 680.)//MANUEL: CHECK: is this correct?
          He_Boltz=exp(preco->Bfact/Trad);
        else
          He_Boltz=exp(680.);

        beta_He = Rup_He*exp(-preco->CL_He/Trad);//MANUEL: NOTE: Trad instead of Tmat
        beta_trip = Rup_trip*exp(-_h_P_*_c_*_L_He_2St_/(_k_B_*Trad));//MANUEL: NOTE: Trad instead of Tmat
      }
      else {//MANUEL: ORIGINAL

        if (preco->Bfact/Tmat < 680.)
          He_Boltz=exp(preco->Bfact/Tmat);//MANUEL: ORIGINAL
        else
          He_Boltz=exp(680.);//MANUEL: ORIGINAL

        beta_He = Rup_He*exp(-preco->CL_He/Tmat);//MANUEL: ORIGINAL
        beta_trip = Rup_trip*exp(-_h_P_*_c_*_L_He_2St_/(_k_B_*Tmat));//MANUEL: ORIGINAL
      }
//MANUEL

      /* equations modified to take into account energy injection from dark matter */
      //C_He=(1. + K_He*_Lambda_He_*n_He*(1.-x_He)*He_Boltz)/(1. + K_He*(_Lambda_He_+Rup_He)*n_He*(1.-x_He)*He_Boltz);

      dy[1] = (((x*x_He*n*Rdown_He - beta_He*(1.-x_He))
               *(1. + K_He*_Lambda_He_*n_He*(1.-x_He)*He_Boltz))
        /(Hz*(1+z)* (1. + K_He*(_Lambda_He_+Rup_He)*n_He*(1.-x_He)*He_Boltz)))*(-pvecback[pba->index_bg_H]); /* in case of energy injection due to DM, we neglect the contribution to helium ionization */

      /* following is from recfast 1.4 */
      /* this correction is not self-consistent when there is energy injection  from dark matter, and leads to nan's  at small redshift (unimportant when reionization takes over before that redshift) */

      if (Heflag >= 3)
        dy[1] +=
          ((x*x_He*n*Rdown_trip
           - (1.-x_He)*3.*beta_trip)
          *CfHe_t/(Hz*(1.+z)))*(-pvecback[pba->index_bg_H]);

    }

    dkdt = (1.+z) * (1.+z) * preco->Nnow * x * _sigma_ * _Mpc_over_m_;
    Rg = (4./3.)*((1.+z)*pba->Omega0_g/pba->Omega0_b)*dkdt;

    Rx = (1.+z) * (1.+z) * pdmde->pref1 * pow(V_rms2/3. + pdmde->pref2*(Tmat + pdmde->pref3*Tex), (pdmde->dpn + 1.)/2.);
    Rxprime = pdmde->mxb*Rx;

    dy[2] = -2*aH*Tmat + 2.*(mub/_m_e_)*Rg*(Trad-Tmat);

    if (pdmde->is_Tb_blind == _FALSE_)
      dy[2] += 2.*S*(mub/mtot)*Rx*(Tex-Tmat);

    dy[3] = -2.*aH*Tex + 2.*Rxprime*(Tmat-Tex);

    if (x < 1.)
      chi_heat = MIN(0.996857*(1.-pow(1.-pow(x,0.300134),1.51035)),1);
    else
      chi_heat = 1.;

    dy[2] += (-2./(3.*_k_B_)*energy_rate*chi_heat/n/(1.+preco->fHe+x)/(Hz*(1.+z)))*(-pvecback[pba->index_bg_H]);
  }

  return _SUCCESS_;
}

//MANUEL
/**
 * This routine solves the DM decoupling equations before recombination, and is inspired by the RECFAST code for recombination.
 *
 * @param ppr   Input: pointer to precision structure
 * @param pba   Input: pointer to background structure
 * @param pth   Input/Output: pointer to thermo structure
 * @param pdmde Input: pointer to dmdecoupling structure
 */

int thermodynamics_dmdecoupling(
                                struct precision * ppr,
                                struct background * pba,
                                struct thermo * pth,
                                struct dmdecoupling * pdmde,
                                struct thermo_vector * pthv,
                                double * pvecback
                               ) {

  // Summary:
  // - define local variables

  double mu_H,x0;
  double zlargest,zmax,zmin,zrecf,zfo,znr,zdec,zUP;
  double zinitial,tinitial;
  double zstart,zend,tstart,tend;
  double z,tau,a;
  double tauH,tau_step,trecf;
  double Tb,dTbdz,Tx,dTxdz;
  int i,j;
  int number_of_tau_steps;

  double * dme_vector;

  int checkpoint=1;

  int last_index_back;
  int index_thv;
  ErrorMsg error_message;

  // Vector of variables to be integrated: Tb & Tx
  double y[2],dy[2];//NOTE: does not include evolution of photon temperature Tg

  // contains all fixed parameters which should be passed to thermodynamics_derivs_with_dmdecoupling
  struct thermodynamics_parameters_and_workspace_dmdec dtpaw;

  //the stiff integrator for the thermodynamics module
  extern int thermo_stiff_ndf15();

  // a growing table (since the number of redshift steps is not known a priori)
  growTable gTable;
  // needed for growing table
  double * pData;
  // needed for growing table
  void * memcopy_result;

  // Definition of useful workspace flags
  // For the treatment of Tb:
  pdmde->is_Tb_blind = pth->is_Tb_blind;
  pdmde->early_blind_Tb = pth->early_blind_Tb;

  // Definition of useful quantities

  pdmde->dmH0 = pba->H0 * _c_ / _Mpc_over_m_;// pdmde->dmH0 is H0 in inverse seconds (while pba->H0 is [H0/c] in inverse Mpcs)

  pdmde->Tnow = pba->T_cmb;

  pdmde->Yp = pth->YHe;//Yp: primordial yield of Helium, a.k.a. mass fraction of Helium: rho_He/(rho_H + rho_He) = m_He/m_H * n_He/(_not4_*n_He + n_H). Since n_He = n_n/2 and n_H = n_p - n_n (where n_p, n_n are the number of protons and neutrons) and since _not4_ =approx=4, then we can rewrite Yp =approx= m_He/m_H * n_He/(2n_n + n_p - n_n) = m_He/m_H * n_He/n_b, where n_b = n_p + n_n is the number of baryons (nucleons, sans electrons)

  pdmde->fHe = pdmde->Yp/(_not4_ * (1.-pdmde->Yp));//He abundance per H number: n_He/n_H

  mu_H = 1./(1. - pdmde->Yp);//weighted mass of nucleons, i.e. total mass of the nucleons, divided by the mass of Hydrogen: (m_He*n_He + m_H*n_H)/(m_H n_H). This is the same as 1/YH.

  pdmde->Nnow = 3.*pdmde->dmH0*pdmde->dmH0*pba->Omega0_b/(8.*_PI_*_G_*mu_H*_m_H_ );//The number density of hydrogen, today: n_H,0, in 1/m^3
  x0 = 1.+2.*pdmde->fHe;//the fraction of electrons, normalized to n_H: n_e/n_H. At this point, all the electrons are free.

  pdmde->mB = pth->mB;//the mass of the baryon with which chi interacts

  // some important names for the DM-decoupling quantities.
  pdmde->dpn = pth->pow_n;
  pdmde->dcn = pth->c_n;
  pdmde->ds0 = pth->sigma_0;
  pdmde->dmx = pth->mx;

  // Other quantities and prefactors
  pdmde->mub = _m_H_*(1. + _not4_*pdmde->fHe)/(1. + pdmde->fHe + x0);//mean molecular weight of the baryons: (rho_H + rho_He)/(n_H + n_He + n_e); rho_e has been ignored because it's tiny

  pdmde->mxb = (pdmde->dmx)/(pdmde->dmx + pdmde->mB/_MeV_over_kg_);//x-baryon reduded mass, divided by the mass of the baryon

  pdmde->muxb = pth->muxb;// chiDM-baryon reduced mass [kg]

  pdmde->pref1 = pth->pref1;
  pdmde->pref2 = pth->pref2;
  pdmde->pref3 = pth->pref3;

  if (pth->thermodynamics_verbose > 1) {

    printf("********************************************\nchiDM-baryon interactions thermodynamics module:\n\tdmdecoupling routine\n\t(see arXiv:1311.2937 and arXiv:1802.06788)\n********************************************\n");

    printf("    chiDM parameters:\n\t mx = %g GeV, sigma_0 = %.2e cm^2, n=%i, c_n=%f, mB=%g GeV\n",pth->mx*1.e-3,pth->sigma_0*1.e4,(int)pth->pow_n,pth->c_n,(1.e-3)*pth->mB/_MeV_over_kg_);

    if ((pth->realistic_Tx == _TRUE_) && (pdmde->dpn <= -3.))
      printf("\t <sigma v>_fo = %.2e cm^3/s\n",pth->sigmav_fo*1.e6);

    if (pdmde->is_Tb_blind == _FALSE_) {
      printf("\t (is_Tb_blind = FALSE): You have selected Tb to respond to Tx at some point in time.\n");

      if (pdmde->early_blind_Tb == _TRUE_)
        printf("\t (early_blind_Tb = TRUE): Tb will evolve independently of Tx in the early Universe, in the dmdecoupling routine. After this routine, it will respond to Tx.\n");
      else
        printf("\t (early_blind_Tb = FALSE): Tb will ALWAYS respond to Tx, even in the early Universe, in the dmdecoupling routine.\n");
    }
    else {
      printf("\t (is_Tb_blind = TRUE): You have selected Tb to ALWAYS be blind to (i.e. ignore) Tx.\n");
      printf("\t (early_blind_Tb = TRUE): This obviously includes the early times considered in the dmdecoupling routine.\n");
    }

    if (pth->realistic_Tx == _TRUE_) {
      printf("\t (realistic_Tx = TRUE): You have selected to start Tx from a realistic value.\n");

      if (pdmde->dpn > -3.)
        printf("\t \t n > -3: Tx=Tb early on down to z_thdec or z_nr, whichever is the latest.\n");
      else
        printf("\t \t n <= -3: Tx will start from its freeze-out value.\n");
    }
    else {
      printf("\t (realistic_Tx = FALSE): You have selected to start Tx from a possibly unrealistic value.\n");

      if (pdmde->dpn > -3.)
        printf("\t \t n > -3: For small mx this could mean a temperature above its mass. In other words, we ignore z_nr.\n");
      else
        printf("\t \t n <= -3: Initial Tx = 0 K.\n");
    }
  }

  // --- Estimates of important redshifts:

  // The larget z possible in CLASS
  zlargest = -1. + 1./ppr->a_ini_over_a_today_default;

  // Start of RECFAST code for recombination
  zrecf = ppr->recfast_z_initial;

  // The maximum z we will pass through the stiff integrator.
  zmax = ppr->z_dmde_max;//The default value is 2*10^-7. I think this value is OK: modes that enter the horizon at this time are k~100 1/Mpc, much larger than those we have data on. So if we get the thermodynamics kinda wrong before this time, it doesn't really matter.

  //The smallest z at which we will start the dmdecoupling routine.
  zmin = ppr->dmde_buffer*zrecf;

  // Redshift at which the DM becomes non-relativistic: T(znr)/m = 1./3
  pdmde->z_nr = pth->z_nr;
  znr = pth->z_nr;

  // DM freeze-out redshift.
  pdmde->z_fo = pth->z_fo;
  zfo = pth->z_fo;

  class_test((zmin > znr),
             pth->error_message,
             "buffer*zrecfast = %.4e > znr = %.4e. This should never happen.",zmin,znr);

  // The value for the upper bound of the redshifts we will consider.
  if (pth->realistic_Tx == _TRUE_) {

    if ((pdmde->ds0 > 0.) && (pdmde->dpn > -3.)) {
      zdec = pth->appx_z_thdec;//approximate redshift of thermal decoupling
      zdec = MIN(zdec, znr);

      zUP = MIN(pth->z_theq_ratio, znr);
      zUP = MAX(zmin, zUP);
    }
    else if (pdmde->dpn <= -3.) {
      zUP = MAX(zmin, zfo);//Just for show: zfo must be larger than zmin (= ppr->dmde_buffer*zrecf)
    }
  }
  else {
    if ((pdmde->ds0 > 0.) && (pdmde->dpn > -3.)) {
      zUP = MAX(zmin, pth->z_theq_ratio);
    }
    else if (pdmde->dpn <= -3.) {

      zUP = zmin;
      // zUP = zmax;
    }
  }

  zUP = MIN(zUP, zmax);

  if (pth->thermodynamics_verbose > 1) {

    printf("\nSome important redshifts:\n\tzlargest=%.4e: largest redshift\n\tzrecf=%.4e: redshift at start of RECFAST\n\n\tznr=%.4e: non-relativistic redshift\n",zlargest,zrecf,znr);//PRINT:

    if (pdmde->dpn > -3.)
      printf("\n\tz_theq_ratio=%.4e: redshift of chiDM-baryon thermal equilibrium (Gamma/H=%.3g)\n\tappx_z_thdec=%.4e: (approximate) redshift of chiDM-baryon thermal decoupling\n",pth->z_theq_ratio,ppr->Rx_H_ratio,pth->appx_z_thdec);//PRINT:

    else if (pth->realistic_Tx == _FALSE_)
      printf("\n\tzfo=%.4e: freeze-out redshift\n",zfo);//PRINT:
  }

  // allocate vector of (tau) values related to dark recombination
  class_alloc(dme_vector,sizeof(double),pth->error_message);

  //create a growTable with gt_init()
  class_call(gt_init(&gTable),
             gTable.error_message,
             pth->error_message);

  //Calculating starting values for the gTable:
  zinitial = zUP;

  class_call(background_tau_of_z(pba,
                                 zinitial,
                                 &tinitial),
             pba->error_message,
             pba->error_message);

  tau = tinitial;
  dme_vector[0] = tau;

  //store these values in growing table
  class_call(gt_add(&gTable,_GT_END_,(void *) dme_vector,sizeof(double)),
             gTable.error_message,
             pth->error_message);

  number_of_tau_steps=1;

  class_call(background_at_tau(pba,
                             tau,
                             pba->short_info,
                             pba->inter_normal,
                             &last_index_back,
                             pvecback),
           pba->error_message,
           pth->error_message);

  a = pvecback[pba->index_bg_a];
  z = -1. + 1./a;

  tauH = 1./a/pvecback[pba->index_bg_H];
  tau_step = tauH;

  tau_step *= 1./((double) ppr->refine);

  tau_step = MAX(tau_step, ppr->smallest_allowed_variation);

  class_call(background_tau_of_z(pba,
                                 zrecf,
                                 &trecf),
             pba->error_message,
             pba->error_message);

  while (tau <= trecf) {

    class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             pth->error_message);

    a = pvecback[pba->index_bg_a];
    z = -1. + 1./a;

    tauH = 1./a/pvecback[pba->index_bg_H];

    tau_step = tauH;
    tau_step *= 1./((double) ppr->refine);

    tau_step = MAX(tau_step, ppr->smallest_allowed_variation);

    tau_step = MIN(trecf-tau, tau_step);

    if (tau_step > 0) {
      tau += tau_step;
      dme_vector[0] = tau;

      class_call(gt_add(&gTable,_GT_END_,(void *) dme_vector,sizeof(double)),
                   gTable.error_message,
                   pth->error_message);

      number_of_tau_steps++;
    }
    else {
      break;
    }
  }

  //allocate tau_vec with the correct size:
  class_alloc(pdmde->tau_vec,number_of_tau_steps*sizeof(double),pth->error_message);
  //allocate z_vec with the correct size:
  class_alloc(pdmde->z_vec,number_of_tau_steps*sizeof(double),pth->error_message);
  //allocate the full dark_recombination_table with the correct size
  class_alloc(pdmde->dmdecoupling_table,number_of_tau_steps*pdmde->dm_size*sizeof(double),pth->error_message);

  pdmde->rt_size = number_of_tau_steps;

  if (pth->thermodynamics_verbose > 3)
    printf("\n\tLength of pdmde->tau_vec, i.e., number of tau steps: pdmde->rt_size=%i\n",pdmde->rt_size);//PRINT:

  //retrieve data stored in the growTable with gt_getPtr()
  class_call(gt_getPtr(&gTable,(void**)&pData),
             gTable.error_message,
             pth->error_message);

  //copy growTable to pdmde->tau_vec table (invert order of lines, so that redshift is growing, like in recombination table)
  for (i=0; i < pdmde->rt_size; i++) {
    memcopy_result = memcpy(pdmde->tau_vec+i*1,pData+i*1,1*sizeof(double));
    class_test(memcopy_result != pdmde->tau_vec+i*1,
               pth->error_message,
               "cannot copy data back to tau_vec table");

  }

  if (pth->thermodynamics_verbose > 3)
    printf("\t -> tau_vec[0]=%f=%.4e, tau_vec[%i]=%f=%.4e\n",pdmde->tau_vec[0],pdmde->tau_vec[0],pdmde->rt_size-1,pdmde->tau_vec[pdmde->rt_size-1],pdmde->tau_vec[pdmde->rt_size-1]);//PRINT:

  //free the growTable with gt_free() , free vector of dao variables
  class_call(gt_free(&gTable),
             gTable.error_message,
             pth->error_message);

  free(dme_vector);

  //allocate memory for vector of redshifts and the boolean variables, stating whether some quantities are printed:
  class_alloc(pthv,sizeof(struct thermo_vector),pth->error_message);

  //define the indices for pthv
  index_thv = 0;
  class_define_index(pthv->index_thv_Tb,_TRUE_,index_thv,1);
  class_define_index(pthv->index_thv_Tx,_TRUE_,index_thv,1);
  pthv->thv_size = index_thv;// = 2; clearly!!!!

  //allocate vectors for storing the values of all these quantities and their redshift-derivatives at a given redshift.
  class_calloc(pthv->y,pthv->thv_size,sizeof(double),pth->error_message);
  class_alloc(pthv->dy,pthv->thv_size*sizeof(double),pth->error_message);

  pdmde->z_vec[pdmde->rt_size-1] = zinitial;

  for (i=1; i<pdmde->rt_size-1; i++) {
    tau = pdmde->tau_vec[i];

    class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             pth->error_message);

    pdmde->z_vec[pdmde->rt_size-1-i] = -1. + 1/pvecback[pba->index_bg_a];
  }

  pdmde->z_vec[0] = ppr->recfast_z_initial;

  if (pth->thermodynamics_verbose > 3)
    printf("\t -> z_vec[0]=%f=%.4e, z_vec[%i]=%f=%.4e\n",pdmde->z_vec[0],pdmde->z_vec[0],pdmde->rt_size-1,pdmde->z_vec[pdmde->rt_size-1],pdmde->z_vec[pdmde->rt_size-1]);//PRINT:

  // - define the fields of the 'thermodynamics_parameters_and_workspace_dmdec' structure
  dtpaw.pba = pba;
  dtpaw.ppr = ppr;
  dtpaw.pdmde = pdmde;
  dtpaw.pvecback = pvecback;
  dtpaw.pthv = pthv;

  // defining the initial conditions:
  Tb = pdmde->Tnow*(1.+zinitial);
  dTbdz = pdmde->Tnow;

  if (pth->realistic_Tx == _TRUE_) {
    if ((pdmde->ds0 > 0.) && (pdmde->dpn > -3.)) {
      if (zUP >= zdec) {
        Tx = Tb;
        dTxdz = dTbdz;
      }
      else {
        Tx = pdmde->Tnow*(1.+zdec)*pow((1.+zinitial)/(1.+zdec),2.);
        dTxdz = 2*Tx/(1.+zinitial);
      }
    }
    else if (pdmde->dpn <= -3.) {
      Tx = pdmde->Tnow*(1.+zfo)*pow((1.+zinitial)/(1.+zfo),2.);
      dTxdz = 2*Tx/(1.+zinitial);
    }
  }
  else {
    if ((pdmde->ds0 > 0.) && (pdmde->dpn > -3.)) {
      if (zUP >= pth->appx_z_thdec) {
        Tx = Tb;
        dTxdz = dTbdz;
      }
      else {
        Tx = pdmde->Tnow*(1.+pth->appx_z_thdec)*pow((1.+zinitial)/(1.+pth->appx_z_thdec),2.);
        dTxdz = 2*Tx/(1.+zinitial);
      }
    }
    else if (pdmde->dpn <= -3.) {
      Tx = 0.;
      dTxdz = 0.;
    }
  }

  y[0] = Tb;
  y[1] = Tx;

  if (pth->thermodynamics_verbose > 1)
    printf("\nFinally:\n\tzinitial=%.4e; tinitial=%.4e Mpc, with initial conditions:\n\tTb=%.2e K; Tx=%.2e K\n\tdTbdz=%.4e, dTxdz=%.4e\n********************************************\n",zinitial,tinitial,Tb,Tx,dTbdz,dTxdz);//PRINT:

  // allocating the first entries of the dark_recombination table

  class_call(background_tau_of_z(pba,
                                 zinitial,
                                 &tinitial),
             pba->error_message,
             error_message);

  class_call(background_at_tau(pba,
                               tinitial,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_Tb) = Tb;
  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_Tx) = Tx;
  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_xe) = x0;
  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_wb) =  _k_B_ * Tb / ( _c_ * _c_ * pdmde->mub );
  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_cb2) = pdmde->dmdecoupling_table[(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_wb]*(1. + 1./3.);

  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_cx2) = pdmde->pref2 * pdmde->pref3 * (Tx + pth->pref_cx2*(1.+zinitial)*dTxdz / 3.);//CORRECT: there is  little blip in cx2 when it matches from tight to numeric (around z_gi_ini).
  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_dkappadtau) = (1.+zinitial)*(1.+zinitial)*pdmde->Nnow*x0*_sigma_*_Mpc_over_m_;

  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size + pdmde->index_dm_Rx) = (1.+zinitial)*(1.+zinitial)*pdmde->pref1*pow(pba->Vrms2/3. + pdmde->pref2*(Tb + pdmde->pref3*Tx), (pdmde->dpn + 1.)/2.);

  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size + pdmde->index_dm_Rxprime) = pth->mxb * pdmde->dmdecoupling_table[(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_Rx];

  *(pdmde->dmdecoupling_table+(pdmde->rt_size-1)*pdmde->dm_size + pdmde->index_dm_dRx) = -pvecback[pba->index_bg_H]*(2./(1.+zinitial) + ((pdmde->dpn + 1.)/2.)*(pdmde->pref2*(dTbdz + pdmde->pref3*dTxdz))/(pba->Vrms2/3. + pdmde->pref2*(Tb + pdmde->pref3*Tx)))*pdmde->dmdecoupling_table[(pdmde->rt_size-1)*pdmde->dm_size+pdmde->index_dm_Rx];

  // START: the loops to solve the temperature equations.

  for(i=0; i<pdmde->rt_size-1; i++) {

    tstart = pdmde->tau_vec[i];
    tend = pdmde->tau_vec[i+1];
    tau = tend;

    zstart = pdmde->z_vec[pdmde->rt_size-1-i];
    zend = pdmde->z_vec[pdmde->rt_size-1-(i+1)];
    z = zend;

    // update the vector of y-variables to the previous iteration; this is used as input in the integrator
    y[0] = pdmde->dmdecoupling_table[(pdmde->rt_size-1-i)*pdmde->dm_size+pdmde->index_dm_Tb];
    y[1] = pdmde->dmdecoupling_table[(pdmde->rt_size-1-i)*pdmde->dm_size+pdmde->index_dm_Tx];

    class_call(thermo_stiff_ndf15(thermodynamics_tau_derivs_dmdecoupling,
                                    tstart,
                                    tend,
                                    y,
                                    2,
                                    &dtpaw,
                                    ppr->tol_dmde_integration,
                                    ppr->smallest_allowed_variation,
                                    pdmde->tau_vec,
                                    pdmde->rt_size,
                                    dmde_thermo_sources,
                                    pth->error_message),
                   pth->error_message,
                   pth->error_message);

    class_call(background_at_tau(pba,
                                 tau,
                                 pba->short_info,
                                 pba->inter_normal,
                                 &last_index_back,
                                 pvecback),
               pba->error_message,
               error_message);

    Tb = pthv->y[pthv->index_thv_Tb];
    Tx = pthv->y[pthv->index_thv_Tx];

    y[0] = Tb;
    y[1] = Tx;

    // - --> store the results in the table
    // results are obtained in order of decreasing z, and stored in order of growing z

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_xe) = x0;
    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_Tb) = Tb;
    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_Tx) = Tx;

    dTbdz = pthv->dy[pthv->index_thv_Tb]/(-pvecback[pba->index_bg_H]);
    dTxdz = pthv->dy[pthv->index_thv_Tx]/(-pvecback[pba->index_bg_H]);

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_wb) = _k_B_ * Tb / ( _c_ * _c_ * pdmde->mub );

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_cb2) = pdmde->dmdecoupling_table[(pdmde->rt_size-1-(i+1))*pdmde->dm_size+pdmde->index_dm_wb] * (1. + (1.+z) * dTbdz / Tb / 3.);

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_cx2) = pdmde->pref2 * pdmde->pref3 * (Tx + pth->pref_cx2*(1.+z) * dTxdz / 3.);//CORRECT: there is  little blip in cx2 when it matches from tight to numeric (around z_gi_ini).

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_dkappadtau) = (1.+z) * (1.+z) * pdmde->Nnow * x0 * _sigma_ * _Mpc_over_m_;

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_Rx) = (1.+z)*(1.+z) * pdmde->pref1 * pow(pba->Vrms2/3. + pdmde->pref2*(Tb + pdmde->pref3*Tx), (pth->pow_n + 1.)/2.);

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_Rxprime) = pth->mxb * pdmde->dmdecoupling_table[(pdmde->rt_size-1-(i+1))*pdmde->dm_size+pdmde->index_dm_Rx];

    *(pdmde->dmdecoupling_table+(pdmde->rt_size-1-(i+1))*pdmde->dm_size + pdmde->index_dm_dRx) = -pvecback[pba->index_bg_H]*(2./(1.+z) + ((pth->pow_n + 1.)/2.)*(pdmde->pref2*(dTbdz + pdmde->pref3*dTxdz))/(pba->Vrms2/3. + pdmde->pref2*(Tb + pdmde->pref3*Tx)))*pdmde->dmdecoupling_table[(pdmde->rt_size-1-(i+1))*pdmde->dm_size+pdmde->index_dm_Rx];

  }

  //cleanup thermo_vector
  class_call(thermo_vector_free(pthv),
             pth->error_message,
             pth->error_message);

  free(pdmde->tau_vec);

  return _SUCCESS_;
}
//MANUEL

//MANUEL
/**
 * Subroutine evaluating the derivative with respect to conformal time of
 * thermodynamical quantities, for the dmdecoupling routine.
 *
 * Computes derivatives of the three variables to integrate: dTb/dtau, dTx/dtau.
 *
 * This is one of the few functions in the code which are passed to
 * the stiff integrator thermo_stiff_ndf15() routine.
 *
 * @param tau                      Input: conformal time
 * @param y                        Input: vector of variable to integrate
 * @param dy                       Output: its derivative (already allocated)
 * @param parameters_and_workspace Input: pointer to fixed parameters (e.g. indices) and workspace (already allocated)
 * @param error_message            Output: error message
 */

int thermodynamics_tau_derivs_dmdecoupling(
                                       double tau,
                                       double * y,
                                       double * dy,
                                       void * parameters_and_workspace,
                                       ErrorMsg error_message
                                       ) {

  /* define local variables */
  double x0,Trad,Tba,Tex;
  double S,mtot;
  double aH,dkdt,Rg,Rx,Rxprime;

  struct thermodynamics_parameters_and_workspace_dmdec * dptpaw;
  struct precision * ppr;
  struct background * pba;
  struct dmdecoupling * pdmde;
  double * pvecback;

  double z;
  int last_index_back;

  dptpaw = parameters_and_workspace;
  ppr = dptpaw->ppr;
  pba = dptpaw->pba;
  pdmde = dptpaw->pdmde;
  pvecback = dptpaw->pvecback;

  class_call(background_at_tau(pba,
                               tau,
                               pba->short_info,
                               pba->inter_normal,
                               &last_index_back,
                               pvecback),
             pba->error_message,
             error_message);

  z = (1./pvecback[pba->index_bg_a]) - 1.;
  aH = pvecback[pba->index_bg_a]*pvecback[pba->index_bg_H];

  x0 = 1.+2.*pdmde->fHe;

  Trad = pdmde->Tnow * (1.+z);
  Tba = y[0];
  Tex = y[1];

  S = pba->Omega0_chi/pba->Omega0_b;
  mtot = (pdmde->dmx*_MeV_over_kg_)/pdmde->mxb;// [kg] total DM+b mass

  dkdt = (1.+z) * (1.+z) * pdmde->Nnow * x0 * _sigma_ * _Mpc_over_m_;
  Rg = (4./3.)*((1.+z)*pba->Omega0_g/pba->Omega0_b)*dkdt;

  Rx = (1.+z) * (1.+z) * pdmde->pref1 * pow(pba->Vrms2/3. + pdmde->pref2*(Tba + pdmde->pref3*Tex), (pdmde->dpn + 1.)/2.);
  Rxprime = pdmde->mxb*Rx;

  if ((pdmde->is_Tb_blind == _TRUE_) || (pdmde->early_blind_Tb == _TRUE_))
    dy[0] = -aH*Tba;
  else
    dy[0] = -2*aH*Tba + 2.*(pdmde->mub/_m_e_)*Rg*(Trad-Tba) + 2.*S*(pdmde->mub/mtot)*Rx*(Tex-Tba);

  dy[1] = -2.*aH*Tex + 2.*Rxprime*(Tba-Tex);

  class_test(Tex < 0., error_message, "Found a point for which Tx < 0. This is clearly WRONG!\nz=%.2e, tau=%.2e\nTex=%.2e, Tba=%.2e\ndTx/dt=%.2e, dTb/dt=%.2e\ndTx/dz=%.2e, dTb/dz=%.2e",z,tau,Tex,Tba,dy[1],dy[0],dy[1]/(-pvecback[pba->index_bg_H]),dy[0]/(-pvecback[pba->index_bg_H]));

  class_test(Tex < 0., error_message, "Found a point for which Tb < 0. This is clearly WRONG!\nz=%.2e, tau=%.2e\nTex=%.2e, Tba=%.2e\ndTx/dt=%.2e, dTb/dt=%.2e\ndTx/dz=%.2e, dTb/dz=%.2e",z,tau,Tex,Tba,dy[1],dy[0],dy[1]/(-pvecback[pba->index_bg_H]),dy[0]/(-pvecback[pba->index_bg_H]));

  return _SUCCESS_;
}
//MANUEL

//MANUEL
/**
 * This routine is inspired by 'perturb_sources()' in perturbations.c and computes the functions necessary for the recombination calculations, namely x_H, x_He, T_b, and T_x.
 *
 * @param tau         Input: conformal time
 * @param y           Input: vector of thermodynamic functions
 * @param dy          Input: vector of redshift-derivatives of thermodynamic functions
 * @param para...     Input/Output: in "Input", all parameters needed by thermo_; in "Output", the final values (source terms)
 * @return            the error status
 */
int thermo_sources(
                    double tau,
                    double * y,
                    double * dy,
                    void * parameters_and_workspace,
                    ErrorMsg error_message
                  ) {

  struct thermodynamics_parameters_and_workspace * ptpaw;
  struct thermo_vector * ppthv;

  //To avoid cumbersome notation
  ptpaw = parameters_and_workspace;
  ppthv = ptpaw->pthv;

  if (ppthv->thv_size == 4) {
    ppthv->y[ppthv->index_thv_H] = y[0];
    ppthv->y[ppthv->index_thv_He] = y[1];
    ppthv->y[ppthv->index_thv_Tb] = y[2];
    ppthv->y[ppthv->index_thv_Tx] = y[3];

    ppthv->dy[ppthv->index_thv_H] = dy[0];
    ppthv->dy[ppthv->index_thv_He] = dy[1];
    ppthv->dy[ppthv->index_thv_Tb] = dy[2];
    ppthv->dy[ppthv->index_thv_Tx] = dy[3];
  }
  else if (ppthv->thv_size == 2) {
    ppthv->y[ppthv->index_thv_Tb] = y[0];
    ppthv->y[ppthv->index_thv_Tx] = y[1];

    ppthv->dy[ppthv->index_thv_Tb] = dy[0];
    ppthv->dy[ppthv->index_thv_Tx] = dy[1];
  }
  else
    printf("There was an error in calling thermo_sources:pptv->thv_size = %i != 2,3,4\n",ppthv->thv_size);

  return _SUCCESS_;
}
//MANUEL

//MANUEL
/**
 * This routine is inspired by 'perturb_sources()' in perturbations.c and computes the functions necessary for the recombination calculations, namely x_H, x_He, T_b, and T_x.
 *
 * @param tau         Input: conformal time
 * @param y           Input: vector of thermodynamic functions
 * @param dy          Input: vector of redshift-derivatives of thermodynamic functions
 * @param para...     Input/Output: in "Input", all parameters needed by thermo_; in "Output", the final values (source terms)
 * @return            the error status
 */
int dmde_thermo_sources(
                    double tau,
                    double * y,
                    double * dy,
                    void * parameters_and_workspace,
                    ErrorMsg error_message
                  ) {

  struct thermodynamics_parameters_and_workspace_dmdec * ptpaw;
  struct thermo_vector * ppthv;

  //To avoid cumbersome notation
  ptpaw = parameters_and_workspace;
  ppthv = ptpaw->pthv;

  if (ppthv->thv_size == 2) {

    ppthv->y[ppthv->index_thv_Tb] = y[0];
    ppthv->y[ppthv->index_thv_Tx] = y[1];

    ppthv->dy[ppthv->index_thv_Tb] = dy[0];
    ppthv->dy[ppthv->index_thv_Tx] = dy[1];

  }
  else
    printf("There was an error in calling thermo_sources:pptv->thv_size = %i != 2,3,4\n",ppthv->thv_size);

  return _SUCCESS_;
}
//MANUEL

//MANUEL
/**
 * This routine is inspired by 'perturb_vector_free()' in perturbations.c and frees the thermo_vector structure.
 *
 * @param pthv        Input: pointer to thermo_vector structure to be freed
 * @return the error status
 */
int thermo_vector_free(
                        struct thermo_vector * pthv
                      ) {

  free(pthv->y);
  free(pthv->dy);
  free(pthv);

  return _SUCCESS_;
}
//MANUEL

/**
 * This routine merges the two tables 'recombination_table' and
 * 'reionization_table' inside the table 'thermodynamics_table', and
 * frees the temporary structures 'recombination' and 'reionization'.
 *
 * @param ppr   Input: pointer to precision structure
 * @param pth   Input/Output: pointer to thermo structure
 * @param preco Input: pointer to filled recombination structure
 * @param preio Input: pointer to reionization structure
 * @return the error status
 */

int thermodynamics_merge_reco_and_reio(
                                       struct precision * ppr,
                                       struct background * pba,
                                       struct thermo * pth,
                                       struct recombination * preco,
                                       struct reionization * preio,
                                       struct dmdecoupling * pdmde//MANUEL
                                       ) {
  /** Summary: */

  /** - define local variables */

  int i,index_th,index_re;
  double x0;
//MANUEL
  int index_dm;
//MANUEL

  /** - first, a little check that the two tables match each other and can be merged */

  if (pth->reio_parametrization != reio_none) {
    class_test(preco->recombination_table[preio->index_reco_when_reio_start*preco->re_size+preco->index_re_z] !=
               preio->reionization_table[(preio->rt_size -1)*preio->re_size+preio->index_re_z],
               pth->error_message,
               "mismatch which should never happen");
  }

  /** - find number of redshift in full table = number in reco + number in reio - overlap */

  pth->tt_size = ppr->recfast_Nz0 + preio->rt_size - preio->index_reco_when_reio_start - 1;

//MANUEL
  if (pth->compute_dm_thermo == _TRUE_)
    pth->tt_size += pdmde->rt_size;
//MANUEL

  /** - add  more points to start earlier in presence of interacting DM */

  if(pba->has_idm_dr == _TRUE_) pth->tt_size += ppr->thermo_Nz1_idm_dr + ppr->thermo_Nz2_idm_dr - 1;

  /** - allocate arrays in thermo structure */

  class_alloc(pth->z_table,pth->tt_size*sizeof(double),pth->error_message);
  class_alloc(pth->thermodynamics_table,pth->th_size*pth->tt_size*sizeof(double),pth->error_message);
  class_alloc(pth->d2thermodynamics_dz2_table,pth->th_size*pth->tt_size*sizeof(double),pth->error_message);

  /** - fill these arrays */

  for (i=0; i < preio->rt_size; i++) {
    pth->z_table[i]=
      preio->reionization_table[i*preio->re_size+preio->index_re_z];
    pth->thermodynamics_table[i*pth->th_size+pth->index_th_xe]=
      preio->reionization_table[i*preio->re_size+preio->index_re_xe];
    pth->thermodynamics_table[i*pth->th_size+pth->index_th_dkappa]=
      preio->reionization_table[i*preio->re_size+preio->index_re_dkappadtau];
    pth->thermodynamics_table[i*pth->th_size+pth->index_th_Tb]=
      preio->reionization_table[i*preio->re_size+preio->index_re_Tb];
    pth->thermodynamics_table[i*pth->th_size+pth->index_th_wb]=
      preio->reionization_table[i*preio->re_size+preio->index_re_wb];
    pth->thermodynamics_table[i*pth->th_size+pth->index_th_cb2]=
      preio->reionization_table[i*preio->re_size+preio->index_re_cb2];
//MANUEL
    if (pth->compute_dm_thermo == _TRUE_) {
      pth->thermodynamics_table[i*pth->th_size+pth->index_th_Tx]=
        preio->reionization_table[i*preio->re_size+preio->index_re_Tx];
      pth->thermodynamics_table[i*pth->th_size+pth->index_th_cx2]=
        preio->reionization_table[i*preio->re_size+preio->index_re_cx2];
      pth->thermodynamics_table[i*pth->th_size+pth->index_th_Rx]=
        preio->reionization_table[i*preio->re_size+preio->index_re_Rx];
      pth->thermodynamics_table[i*pth->th_size+pth->index_th_Rxprime]=
        preio->reionization_table[i*preio->re_size+preio->index_re_Rxprime];
      pth->thermodynamics_table[i*pth->th_size+pth->index_th_dRx]=
        preio->reionization_table[i*preio->re_size+preio->index_re_dRx];
    }
//MANUEL
  }
  for (i=0; i < ppr->recfast_Nz0 - preio->index_reco_when_reio_start - 1; i++) {
    index_th=i+preio->rt_size;
    index_re=i+preio->index_reco_when_reio_start+1;
    pth->z_table[index_th]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_z];
    pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_xe]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_xe];
    pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_dkappa]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_dkappadtau];
    pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Tb]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_Tb];
    pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_wb]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_wb];
    pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_cb2]=
      preco->recombination_table[index_re*preco->re_size+preco->index_re_cb2];
//MANUEL
    if (pth->compute_dm_thermo == _TRUE_) {
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Tx]=
        preco->recombination_table[index_re*preco->re_size+preco->index_re_Tx];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_cx2]=
        preco->recombination_table[index_re*preco->re_size+preco->index_re_cx2];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Rx]=
        preco->recombination_table[index_re*preco->re_size+preco->index_re_Rx];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Rxprime]=
        preco->recombination_table[index_re*preco->re_size+preco->index_re_Rxprime];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_dRx]=
        preco->recombination_table[index_re*preco->re_size+preco->index_re_dRx];
    }
//MANUEL
  }

//MANUEL
  /* In the presence of chiDM (IDM-b): */
  if (pth->compute_dm_thermo == _TRUE_) {
    for (i=0; i < pdmde->rt_size; i++) {
      index_th=i+preio->rt_size + ppr->recfast_Nz0 - preio->index_reco_when_reio_start - 1;
      index_dm=i;
      pth->z_table[index_th]=
        pdmde->z_vec[index_dm];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_xe]=
        pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_xe];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_dkappa]=
        pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_dkappadtau];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Tb]=
        pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_Tb];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_wb]=
        pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_wb];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_cb2]=
        pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_cb2];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Tx]=
          pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_Tx];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_cx2]=
          pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_cx2];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Rx]=
          pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_Rx];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Rxprime]=
          pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_Rxprime];
      pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_dRx]=
          pdmde->dmdecoupling_table[index_dm*pdmde->dm_size+pdmde->index_dm_dRx];
    }
  }
//MANUEL

  /** - add more points at larger redshift in presence of interacting
        DM. This is necessary because the value of integrated
        quantitites like tau_idm_dr or tau_idr will then be computed
        exactly up to high redshift. With extrapolations in
        thermodynamics_at_z() we could not obtain this. */

    if(pba->has_idm_dr == _TRUE_){

      for (i=0; i<ppr->thermo_Nz2_idm_dr+ppr->thermo_Nz1_idm_dr-1; i++){

        /* with an intermediate step Delta z = (thermo_z_initial_idm_dr-recfast_z_initial)/thermo_Nz1_idm_dr/thermo_Nz1_idm_dr */
        if (i<ppr->thermo_Nz2_idm_dr-1) {
          index_th=i+preio->rt_size+ppr->recfast_Nz0 - preio->index_reco_when_reio_start - 1;
          pth->z_table[index_th]= ppr->recfast_z_initial + ((double)i+1.) * (ppr->thermo_z_initial_idm_dr - ppr->recfast_z_initial) / (double)ppr->thermo_Nz1_idm_dr / (double)ppr->thermo_Nz2_idm_dr;
        }
        /* with a large step Delta z  = (thermo_z_initial_idm_dr-recfast_z_initial)/thermo_Nz1_idm_dr */
        else {
          index_th=(i-ppr->thermo_Nz2_idm_dr+1)+preio->rt_size+ppr->recfast_Nz0 - preio->index_reco_when_reio_start - 1 + ppr->thermo_Nz2_idm_dr - 1;
          pth->z_table[index_th]= ppr->recfast_z_initial + ((double)(i-ppr->thermo_Nz2_idm_dr+1)+1.) * (ppr->thermo_z_initial_idm_dr - ppr->recfast_z_initial) / (double)ppr->thermo_Nz1_idm_dr;
        }
        /* same extrapolation formulas as in thermodynamics_at_z() */
        x0=pth->thermodynamics_table[(preio->rt_size+ppr->recfast_Nz0 - preio->index_reco_when_reio_start - 2)*pth->th_size+pth->index_th_xe];
        pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_xe]=x0;
        pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_dkappa]=(1.+pth->z_table[index_th]) * (1.+pth->z_table[index_th]) * pth->n_e * x0 * _sigma_ * _Mpc_over_m_;
        pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_Tb]=pba->T_cmb*(1.+pth->z_table[index_th]);
        pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_wb]=_k_B_ / ( _c_ * _c_ * _m_H_ ) * (1. + (1./_not4_ - 1.) * pth->YHe + x0 * (1.-pth->YHe)) * pba->T_cmb * (1.+pth->z_table[index_th]);
        pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_cb2]=pth->thermodynamics_table[index_th*pth->th_size+pth->index_th_wb] * 4./3.;
      }

  }

  /** - free the temporary structures */

  free(preco->recombination_table);

  if (pth->reio_parametrization != reio_none)
    free(preio->reionization_table);

//MANUEL
  if (pth->compute_dm_thermo == _TRUE_) {
    free(pdmde->dmdecoupling_table);
    free(pdmde->z_vec);
  }
//MANUEL

  return _SUCCESS_;
}

/**
 * Subroutine for formatting thermodynamics output
 */

int thermodynamics_output_titles(struct background * pba,
                                 struct thermo *pth,
                                 char titles[_MAXTITLESTRINGLENGTH_]
                                 ){

  class_store_columntitle(titles,"z",_TRUE_);
  class_store_columntitle(titles,"conf. time [Mpc]",_TRUE_);
  class_store_columntitle(titles,"x_e",_TRUE_);
  class_store_columntitle(titles,"kappa' [Mpc^-1]",_TRUE_);
  //class_store_columntitle(titles,"kappa''",_TRUE_);
  //class_store_columntitle(titles,"kappa'''",_TRUE_);
  class_store_columntitle(titles,"exp(-kappa)",_TRUE_);
  class_store_columntitle(titles,"g [Mpc^-1]",_TRUE_);
  //class_store_columntitle(titles,"g'",_TRUE_);
  //class_store_columntitle(titles,"g''",_TRUE_);
  class_store_columntitle(titles,"Tb [K]",_TRUE_);
  class_store_columntitle(titles,"w_b",_TRUE_);
  class_store_columntitle(titles,"c_b^2",_TRUE_);
  class_store_columntitle(titles,"tau_d",_TRUE_);
  //class_store_columntitle(titles,"max. rate",_TRUE_);
  class_store_columntitle(titles,"r_d",pth->compute_damping_scale);

//MANUEL
  class_store_columntitle(titles,"Tx [K]",pth->compute_dm_thermo);
  class_store_columntitle(titles,"c_x^2",pth->compute_dm_thermo)
  class_store_columntitle(titles,"R_x",pth->compute_dm_thermo);
  class_store_columntitle(titles,"R_x^prime",pth->compute_dm_thermo);
  class_store_columntitle(titles,"dRx",pth->compute_dm_thermo);
//MANUEL

  if(pba->has_idm_dr == _TRUE_){
    class_store_columntitle(titles,"dmu_idm_dr",_TRUE_);
    //class_store_columntitle(titles,"ddmu_idm_dr",_TRUE_);
    //class_store_columntitle(titles,"dddmu_idm_dr",_TRUE_);
    class_store_columntitle(titles,"tau_idm_dr",_TRUE_);
    class_store_columntitle(titles,"tau_idr",_TRUE_);
    class_store_columntitle(titles,"g_idm_dr [Mpc^-1]",_TRUE_);
    class_store_columntitle(titles,"c_idm_dr^2",_TRUE_);
    class_store_columntitle(titles,"T_idm_dr",_TRUE_);
    class_store_columntitle(titles,"dmu_idr",_TRUE_);
  }

  return _SUCCESS_;
}

int thermodynamics_output_data(struct background * pba,
                               struct thermo *pth,
                               int number_of_titles,
                               double *data
                               ){

  int index_z, storeidx;
  double *dataptr, *pvecthermo;
  double z,tau;

  //  pth->number_of_thermodynamics_titles = get_number_of_titles(pth->thermodynamics_titles);
  //pth->size_thermodynamics_data = pth->number_of_thermodynamics_titles*pth->tt_size;


  /* Store quantities: */
  for (index_z=0; index_z<pth->tt_size; index_z++){
    dataptr = data + index_z*number_of_titles;
    pvecthermo = pth->thermodynamics_table+index_z*pth->th_size;
    z = pth->z_table[index_z];
    storeidx=0;

    class_call(background_tau_of_z(
                                   pba,
                                   z,
                                   &tau
                                   ),
               pba->error_message,
               pth->error_message);

    class_store_double(dataptr,z,_TRUE_,storeidx);
    class_store_double(dataptr,tau,_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_xe],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_dkappa],_TRUE_,storeidx);
    //class_store_double(dataptr,pvecthermo[pth->index_th_ddkappa],_TRUE_,storeidx);
    //class_store_double(dataptr,pvecthermo[pth->index_th_dddkappa],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_exp_m_kappa],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_g],_TRUE_,storeidx);
    //class_store_double(dataptr,pvecthermo[pth->index_th_dg],_TRUE_,storeidx);
    //class_store_double(dataptr,pvecthermo[pth->index_th_ddg],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_Tb],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_wb],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_cb2],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_tau_d],_TRUE_,storeidx);
    //class_store_double(dataptr,pvecthermo[pth->index_th_rate],_TRUE_,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_r_d],pth->compute_damping_scale,storeidx);

//MANUEL
    class_store_double(dataptr,pvecthermo[pth->index_th_Tx],pth->compute_dm_thermo,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_cx2],pth->compute_dm_thermo,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_Rx],pth->compute_dm_thermo,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_Rxprime],pth->compute_dm_thermo,storeidx);
    class_store_double(dataptr,pvecthermo[pth->index_th_dRx],pth->compute_dm_thermo,storeidx);
//MANUEL

    if(pba->has_idm_dr == _TRUE_){
      class_store_double(dataptr,pvecthermo[pth->index_th_dmu_idm_dr],_TRUE_,storeidx);
      //class_store_double(dataptr,pvecthermo[pth->index_th_ddmu_idm_dr],_TRUE_,storeidx);
      //class_store_double(dataptr,pvecthermo[pth->index_th_dddmu_idm_dr],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_tau_idm_dr],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_tau_idr],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_g_idm_dr],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_cidm_dr2],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_Tidm_dr],_TRUE_,storeidx);
      class_store_double(dataptr,pvecthermo[pth->index_th_dmu_idr],_TRUE_,storeidx);
    }
  }

  return _SUCCESS_;
}

int thermodynamics_tanh(double x,
                        double center,
                        double before,
                        double after,
                        double width,
                        double * result) {

  *result = before + (after-before)*(tanh((x-center)/width)+1.)/2.;

  return _SUCCESS_;}
