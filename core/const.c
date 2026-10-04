/* const: the physical constants (unit 004).
 *
 * Which constants exist follows the 35s's constant menu (UG p.4-8, 4-9); every value is ours: the
 * CODATA 2022 recommended value (NIST, physics.nist.gov/cuu/Constants, allascii.txt fetched
 * 2026-10-03), with its relative standard uncertainty. Since the 2019 SI, c, h, e, k and N_A are
 * exact; the constants derived from them only (hbar, phi0, G0, sigma, C1, C2, Vm) are exact too and
 * are given here rounded to 34 digits (computed with mpmath at 80 digits, checked against NIST's
 * printed leading digits). R = N_A k and F = N_A e are exact finite decimals.
 */
#include <string.h>

#include "const.h"

static const struct {
    const char *name, *value;
} CONSTANTS[] = {
    {"c", "299792458"},                                    /* speed of light in vacuum, m s^-1; exact (SI 2019) */
    {"g", "9.80665"},                                      /* standard acceleration of gravity, m s^-2; exact (conventional) */
    {"G", "6.67430E-11"},                                  /* Newtonian constant of gravitation, m^3 kg^-1 s^-2; 2.2E-5 */
    {"Vm", "0.02241396954501413773501110288675056"},       /* molar volume of ideal gas (273.15 K, 101.325 kPa), m^3 mol^-1; exact, R T / p rounded to 34 digits */
    {"NA", "6.02214076E23"},                               /* Avogadro constant, mol^-1; exact (SI 2019) */
    {"Rinf", "10973731.568157"},                           /* Rydberg constant, m^-1; 1.1E-12 */
    {"qe", "1.602176634E-19"},                             /* elementary charge, C; exact (SI 2019) */
    {"me", "9.1093837139E-31"},                            /* electron mass, kg; 3.1E-10 */
    {"mp", "1.67262192595E-27"},                           /* proton mass, kg; 3.1E-10 */
    {"mn", "1.67492750056E-27"},                           /* neutron mass, kg; 5.1E-10 */
    {"mmu", "1.883531627E-28"},                            /* muon mass, kg; 2.2E-8 */
    {"k", "1.380649E-23"},                                 /* Boltzmann constant, J K^-1; exact (SI 2019) */
    {"h", "6.62607015E-34"},                               /* Planck constant, J Hz^-1; exact (SI 2019) */
    {"hbar", "1.054571817646156391262428003302281E-34"},   /* reduced Planck constant h / 2 pi, J s; exact, rounded to 34 digits */
    {"phi0", "2.067833848461929323081115412147497E-15"},   /* magnetic flux quantum h / 2e, Wb; exact, rounded to 34 digits */
    {"a0", "5.29177210544E-11"},                           /* Bohr radius, m; 1.5E-10 */
    {"eps0", "8.8541878188E-12"},                          /* vacuum electric permittivity, F m^-1; 1.6E-10 */
    {"R", "8.31446261815324"},                             /* molar gas constant N_A k, J mol^-1 K^-1; exact */
    {"F", "96485.3321233100184"},                          /* Faraday constant N_A e, C mol^-1; exact */
    {"u", "1.66053906892E-27"},                            /* atomic mass constant, kg; 3.1E-10 */
    {"mu0", "1.25663706127E-6"},                           /* vacuum magnetic permeability, N A^-2; 1.6E-10 */
    {"muB", "9.2740100657E-24"},                           /* Bohr magneton, J T^-1; 3.1E-10 */
    {"muN", "5.0507837393E-27"},                           /* nuclear magneton, J T^-1; 3.2E-10 */
    {"mup", "1.41060679545E-26"},                          /* proton magnetic moment, J T^-1; 4.3E-10 */
    {"mue", "-9.2847646917E-24"},                          /* electron magnetic moment, J T^-1; 3.1E-10 */
    {"mun", "-9.6623653E-27"},                             /* neutron magnetic moment, J T^-1; 2.4E-7 */
    {"mumu", "-4.49044830E-26"},                           /* muon magnetic moment, J T^-1; 2.2E-8 */
    {"re", "2.8179403205E-15"},                            /* classical electron radius, m; 4.6E-10 */
    {"Z0", "376.730313412"},                               /* characteristic impedance of vacuum, ohm; 1.6E-10 */
    {"lambdac", "2.42631023538E-12"},                      /* Compton wavelength, m; 3.1E-10 */
    {"lambdacn", "1.31959090382E-15"},                     /* neutron Compton wavelength, m; 5.1E-10 */
    {"lambdacp", "1.32140985360E-15"},                     /* proton Compton wavelength, m; 3.1E-10 */
    {"alpha", "7.2973525643E-3"},                          /* fine-structure constant; 1.5E-10 */
    {"sigma", "5.670374419184429453970996731889231E-8"},   /* Stefan-Boltzmann constant, W m^-2 K^-4; exact, rounded to 34 digits */
    {"t", "273.15"},                                       /* Celsius zero in kelvin, K; exact (definition) */
    {"atm", "101325"},                                     /* standard atmosphere, Pa; exact (conventional) */
    {"gammap", "2.6752218708E8"},                          /* proton gyromagnetic ratio, s^-1 T^-1; 4.1E-10 */
    {"C1", "3.741771852192758011367155555929985E-16"},     /* first radiation constant 2 pi h c^2, W m^2; exact, rounded to 34 digits */
    {"C2", "0.01438776877503933802146671601543912"},       /* second radiation constant h c / k, m K; exact, rounded to 34 digits */
    {"G0", "0.00007748091729863650646680823323308764"},    /* conductance quantum 2e^2 / h, S; exact, rounded to 34 digits */
    {"e", "2.718281828459045235360287471352662"},          /* base of natural logarithms (mathematics, not CODATA); rounded to 34 digits */
};

int ab_const_count(void)
{
    return (int)(sizeof CONSTANTS / sizeof CONSTANTS[0]);
}

int ab_const_index(const char *name)
{
    for (int i = 0; i < ab_const_count(); i++)
        if (strcmp(CONSTANTS[i].name, name) == 0) return i;
    return -1;
}

const char *ab_const_name(int i)
{
    return (i >= 0 && i < ab_const_count()) ? CONSTANTS[i].name : NULL;
}

const char *ab_const_symbol(int i)
{
    return ab_const_name(i);
}

abn_status ab_const_value(abn *out, int i)
{
    if (i < 0 || i >= ab_const_count()) return ABN_DOMAIN;
    return abn_from_text(out, CONSTANTS[i].value);
}
