/*******************************************************************************
 * This file is part of SWIFT.
 * Coypright (c) 2016 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#ifndef SWIFT_CONST_STARS_IO_H
#define SWIFT_CONST_STARS_IO_H

#include "io_properties.h"
#include "stars_part.h"

/**
 * @brief Specifies which s-particle fields to read from a dataset
 *
 * @param sparts The s-particle array.
 * @param list The list of i/o properties to read.
 * @param num_fields The number of i/o fields to read.
 */
INLINE static void stars_read_particles(struct spart *sparts,
                                        struct io_props *list,
                                        int *num_fields) {

  /* Say how much we want to read */
  *num_fields = 6;

  /* List what we want to read */
  list[0] = io_make_input_field("Coordinates", DOUBLE, 3, COMPULSORY,
                                UNIT_CONV_LENGTH, sparts, x);
  list[1] = io_make_input_field("Velocities", FLOAT, 3, COMPULSORY,
                                UNIT_CONV_SPEED, sparts, v);
  list[2] = io_make_input_field("Masses", FLOAT, 1, COMPULSORY, UNIT_CONV_MASS,
                                sparts, mass);
  /* Temporary addition, discuss with Folkert what to do about initial mass
   * when sparts are read from ICs. */
  list[3] = io_make_input_field("Masses", FLOAT, 1, COMPULSORY, UNIT_CONV_MASS,
                                sparts, mass_init);
  list[4] = io_make_input_field("ParticleIDs", LONGLONG, 1, COMPULSORY,
                                UNIT_CONV_NO_UNITS, sparts, id);
  list[5] = io_make_input_field("SmoothingLength", FLOAT, 1, OPTIONAL,
                                UNIT_CONV_LENGTH, sparts, h);
}

/**
 * @brief Specifies which s-particle fields to write to a dataset
 *
 * @param sparts The s-particle array.
 * @param list The list of i/o properties to write.
 * @param num_fields The number of i/o fields to write.
 */
INLINE static void stars_write_particles(const struct spart *sparts,
                                         struct io_props *list,
                                         int *num_fields) {

  /* Say how much we want to read */
  *num_fields = 5;

  /* List what we want to read */
  list[0] = io_make_output_field("Coordinates", DOUBLE, 3, UNIT_CONV_LENGTH,
                                 sparts, x);
  list[1] =
      io_make_output_field("Velocities", FLOAT, 3, UNIT_CONV_SPEED, sparts, v);
  list[2] =
      io_make_output_field("Masses", FLOAT, 1, UNIT_CONV_MASS, sparts, mass);
  list[3] = io_make_output_field("ParticleIDs", LONGLONG, 1, UNIT_CONV_NO_UNITS,
                                 sparts, id);
  list[4] = io_make_output_field("SmoothingLength", FLOAT, 1, UNIT_CONV_LENGTH,
                                 sparts, h);
}

/**
 * @brief Initialize the global properties of the stars scheme.
 *
 * By default, takes the values provided by the hydro.
 *
 * @param sp The #stars_props.
 * @param phys_const The physical constants in the internal unit system.
 * @param us The internal unit system.
 * @param params The parsed parameters.
 * @param p The already read-in properties of the hydro scheme.
 */
INLINE static void stars_props_init(struct stars_props *sp,
                                    const struct phys_const *phys_const,
                                    const struct unit_system *us,
                                    struct swift_params *params,
                                    const struct hydro_props *p,
				    const struct cosmology *cosmo) {

  /* Kernel properties */
  sp->eta_neighbours = parser_get_opt_param_float(
      params, "Stars:resolution_eta", p->eta_neighbours);

  /* Tolerance for the smoothing length Newton-Raphson scheme */
  sp->h_tolerance =
      parser_get_opt_param_float(params, "Stars:h_tolerance", p->h_tolerance);

  /* Get derived properties */
  sp->target_neighbours = pow_dimension(sp->eta_neighbours) * kernel_norm;
  const float delta_eta = sp->eta_neighbours * (1.f + sp->h_tolerance);
  sp->delta_neighbours =
      (pow_dimension(delta_eta) - pow_dimension(sp->eta_neighbours)) *
      kernel_norm;

  /* Maximal smoothing length */
  sp->h_max = parser_get_opt_param_float(params, "Stars:h_max", p->h_max);

  /* Number of iterations to converge h */
  sp->max_smoothing_iterations = parser_get_opt_param_int(
      params, "Stars:max_ghost_iterations", p->max_smoothing_iterations);

  /* Time integration properties */
  const float max_volume_change =
      parser_get_opt_param_float(params, "Stars:max_volume_change", -1);
  if (max_volume_change == -1)
    sp->log_max_h_change = p->log_max_h_change;
  else
    sp->log_max_h_change = logf(powf(max_volume_change, hydro_dimension_inv));

  /* Set stellar evolution parameters */
  sp->deltaT_desired = 3.16228e7 / units_cgs_conversion_factor(us,UNIT_CONV_TEMPERATURE);
  sp->temp_to_u_factor = phys_const->const_boltzmann_k / (p->mu_ionised * (hydro_gamma_minus_one) * phys_const->const_proton_mass);
  // Why is this one here? copied from EAGLE...
  sp->SNIa_energy_fraction = 1.0e0;
  sp->total_energy_SNe = 1.0e51/units_cgs_conversion_factor(us,UNIT_CONV_ENERGY);

  message("du %.5e in cgs %.5e", sp->deltaT_desired * sp->temp_to_u_factor, sp->deltaT_desired * sp->temp_to_u_factor * units_cgs_conversion_factor(us,UNIT_CONV_ENERGY)/units_cgs_conversion_factor(us,UNIT_CONV_MASS));
  
  /* Set to 1 if using continuous heating, 0 for stochastic  */
  sp->continuous_heating = parser_get_opt_param_int(params, "Stars:continuous_heating", 0);

  // CHANGE NAME. CURRENTLY SAME AS IN EAGLE
  sp->units_factor2 = sp->total_energy_SNe * cosmo->h;
  sp->units_factor1 = sp->units_factor2 / sp->temp_to_u_factor;

  sp->feedback_timescale = parser_get_param_float(params, "Stars:feedback_timescale");

  // CHANGE THIS TO BE CONSISTENT WITH RAND MAX USED IN STAR FORMATION
  sp->inv_rand_max = 1.0/RAND_MAX;

  sp->const_solar_mass = phys_const->const_solar_mass;

}

/**
 * @brief Print the global properties of the stars scheme.
 *
 * @param sp The #stars_props.
 */
INLINE static void stars_props_print(const struct stars_props *sp) {

  /* Now stars */
  message("Stars kernel: %s with eta=%f (%.2f neighbours).", kernel_name,
          sp->eta_neighbours, sp->target_neighbours);

  message("Stars relative tolerance in h: %.5f (+/- %.4f neighbours).",
          sp->h_tolerance, sp->delta_neighbours);

  message(
      "Stars integration: Max change of volume: %.2f "
      "(max|dlog(h)/dt|=%f).",
      pow_dimension(expf(sp->log_max_h_change)), sp->log_max_h_change);

  if (sp->h_max != FLT_MAX)
    message("Maximal smoothing length allowed: %.4f", sp->h_max);

  message("Maximal iterations in ghost task set to %d",
          sp->max_smoothing_iterations);
}

#if defined(HAVE_HDF5)
INLINE static void stars_props_print_snapshot(hid_t h_grpstars,
                                              const struct stars_props *sp) {

  io_write_attribute_s(h_grpstars, "Kernel function", kernel_name);
  io_write_attribute_f(h_grpstars, "Kernel target N_ngb",
                       sp->target_neighbours);
  io_write_attribute_f(h_grpstars, "Kernel delta N_ngb", sp->delta_neighbours);
  io_write_attribute_f(h_grpstars, "Kernel eta", sp->eta_neighbours);
  io_write_attribute_f(h_grpstars, "Smoothing length tolerance",
                       sp->h_tolerance);
  io_write_attribute_f(h_grpstars, "Maximal smoothing length", sp->h_max);
  io_write_attribute_f(h_grpstars, "Volume log(max(delta h))",
                       sp->log_max_h_change);
  io_write_attribute_f(h_grpstars, "Volume max change time-step",
                       pow_dimension(expf(sp->log_max_h_change)));
  io_write_attribute_i(h_grpstars, "Max ghost iterations",
                       sp->max_smoothing_iterations);
}
#endif

/**
 * @brief Write a #stars_props struct to the given FILE as a stream of bytes.
 *
 * @param p the struct
 * @param stream the file stream
 */
INLINE static void stars_props_struct_dump(const struct stars_props *p,
                                           FILE *stream) {
  restart_write_blocks((void *)p, sizeof(struct stars_props), 1, stream,
                       "starsprops", "stars props");
}

/**
 * @brief Restore a stars_props struct from the given FILE as a stream of
 * bytes.
 *
 * @param p the struct
 * @param stream the file stream
 */
INLINE static void stars_props_struct_restore(const struct stars_props *p,
                                              FILE *stream) {
  restart_read_blocks((void *)p, sizeof(struct stars_props), 1, stream, NULL,
                      "stars props");
}

#endif /* SWIFT_DEFAULT_STAR_IO_H */
