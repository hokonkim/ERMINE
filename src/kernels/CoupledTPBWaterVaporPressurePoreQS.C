#include "CoupledTPBWaterVaporPressurePoreQS.h"
#include "Function.h"
#include <cmath>

registerMooseObject("ermineApp", CoupledTPBWaterVaporPressurePoreQS);


template<>
InputParameters validParams<CoupledTPBWaterVaporPressurePoreQS>()
{
  InputParameters params = validParams<Kernel>();
  params.addClassDescription("Fully Coupled TPB reaction source kernel in sinh() form for WaterVapor concentration in pore. j = 2 * j0 * sinh(coef * eta)");
  // params.addRequiredParam<Real>("s0", "Exchange volumetric current density rate (A/cm^3)");
  params.addParam<Real>("z", 4.0, "Electron number (num of electrons transferred)");
  params.addParam<Real>("F", 96485.33289, "Faraday constant (C/mol)");
  params.addParam<Real>("R", 8.3144598, "Gas constant (J/K/mol)");
  params.addParam<Real>("T", 1073.0, "Temperature (T)");
  params.addParam<Real>("Kf", 7.638e8, "Equilibrium constant for H2 + 1/2O2 = H2O at 1073 K");
  params.addRequiredParam<Real>("p_total", "Total pressure of Hydrogen and Water Vapor at the anode (atm)");
  // params.addRequiredParam<FunctionName>("function_phi_Ni", "Function for the Ni potential");
  // params.addRequiredParam<Real>("pO2_CE", "Oxygen partial pressure at the counter electrode (atm)");
  params.addRequiredParam<MaterialPropertyName>("s0", "Exchange volumetric current density rate (A/cm^3)");
  params.addRequiredParam<MaterialPropertyName>("pO2_CE", "Oxygen partial pressure at the counter electrode (atm) (material property)");
  params.addRequiredCoupledVar("phi_YSZ", "The coupled potential variable in eta = potential_rev - (phi_YSZ - phi_Ni)");
  params.addRequiredCoupledVar("phi_Ni", "The coupled potential variable in eta = potential_rev - (phi_YSZ - phi_Ni)");
  return params;
}

CoupledTPBWaterVaporPressurePoreQS::CoupledTPBWaterVaporPressurePoreQS(const InputParameters & parameters) :
    Kernel(parameters),
    // _s0(getParam<Real>("s0")),
    _z(getParam<Real>("z")),
    _F(getParam<Real>("F")),
    _R(getParam<Real>("R")),
    _T(getParam<Real>("T")),
    _Kf(getParam<Real>("Kf")),
    _p_total(getParam<Real>("p_total")),
    // _func_phi_Ni(getFunction("function_phi_Ni")),
    // _pO2_CE(getParam<Real>("pO2_CE")),
    _s0(getMaterialProperty<Real>("s0")),
    _pO2_CE(getMaterialProperty<Real>("pO2_CE")),
    _num_phi_YSZ(coupled("phi_YSZ")),
    _phi_YSZ(coupledValue("phi_YSZ")),
    _num_phi_Ni(coupled("phi_Ni")),
    _phi_Ni(coupledValue("phi_Ni"))
{
}

// Weak form is (somthing) = 0
// What I need to implement is the term in (something)

Real
CoupledTPBWaterVaporPressurePoreQS::computeQpResidual()
{
  Real b       = _z * _F / _R / _T;
  Real E_conc  = _R * _T / 4 / _F * log(_pO2_CE[_qp] / (pow (_u[_qp]/((_p_total-_u[_qp]) * _Kf), 2.0)));
  // Real phi_Ni  = _func_phi_Ni.value(_t, _q_point[_qp]);
  Real eta     = E_conc - (_phi_YSZ[_qp] - _phi_Ni[_qp]);

  Real res     = -0.5 * (2 * _s0[_qp] * sinh(0.5 * b * eta)) / _z / _F; // four electrons transferred (use this when 2 mole hydrogen as fuel)
  return res * _test[_i][_qp];
}

// Jacobian is the derivative of residual with respect to other coupled variables

Real
CoupledTPBWaterVaporPressurePoreQS::computeQpJacobian()
{
  Real b       = _z * _F / _R / _T;
  Real E_conc  = _R * _T / 4 / _F * log(_pO2_CE[_qp] / (pow (_u[_qp]/((_p_total-_u[_qp]) * _Kf), 2.0)));
  // Real phi_Ni  = _func_phi_Ni.value(_t, _q_point[_qp]);
  Real eta     = E_conc - (_phi_YSZ[_qp] - _phi_Ni[_qp]);

  Real ds_deta = b * _s0[_qp] * cosh(0.5 * b * eta);
  Real deta_dE = 1;
  Real dE_dpO2 = -1 / (b * (pow (_u[_qp]/((_p_total - _u[_qp]) * _Kf), 2.0)));
  Real dpO2_dpH2O = ((2 * _u[_qp]) / (pow(_p_total - _u[_qp], 2.0) * pow(_Kf, 2.0))) * (1 + (_u[_qp] / (_p_total - _u[_qp])));

  Real jac     = -0.5 * (ds_deta * deta_dE * dE_dpO2 * dpO2_dpH2O) / _z / _F; // four electrons transferred (use this when 2 mole hydrogen as fuel)
  return jac * _test[_i][_qp] * _phi[_j][_qp];
}

Real
CoupledTPBWaterVaporPressurePoreQS::computeQpOffDiagJacobian(unsigned int jvar)
{
  if (jvar == _num_phi_YSZ)
  {
    Real b       = _z * _F / _R / _T;
    Real E_conc  = _R * _T / 4 / _F * log(_pO2_CE[_qp] / (pow (_u[_qp]/((_p_total-_u[_qp]) * _Kf), 2.0)));
    // Real phi_Ni  = _func_phi_Ni.value(_t, _q_point[_qp]);
    Real eta     = E_conc - (_phi_YSZ[_qp] - _phi_Ni[_qp]);

    Real ds_deta   = b * _s0[_qp] * cosh(0.5 * b * eta);
    Real deta_dphi = -1;

    Real jac       = -0.5 * (ds_deta * deta_dphi) / _z / _F; // four electrons transferred (use this when 2 mole hydrogen as fuel)
    return jac * _test[_i][_qp] * _phi[_j][_qp];
  }
  else if (jvar == _num_phi_Ni)
  {
    Real b       = _z * _F / _R / _T;
    Real E_conc  = _R * _T / 4 / _F * log(_pO2_CE[_qp] / (pow (_u[_qp]/((_p_total-_u[_qp]) * _Kf), 2.0)));
    // Real phi_Ni  = _func_phi_Ni.value(_t, _q_point[_qp]);
    Real eta     = E_conc - (_phi_YSZ[_qp] - _phi_Ni[_qp]);

    Real ds_deta   = b * _s0[_qp] * cosh(0.5 * b * eta);
    Real deta_dNi  = 1;

    Real jac       = -0.5 * (ds_deta * deta_dNi) / _z / _F; // four electrons transferred (use this when 2 mole hydrogen as fuel)
    return jac * _test[_i][_qp] * _phi[_j][_qp];
  }
  else return 0.0;
}
