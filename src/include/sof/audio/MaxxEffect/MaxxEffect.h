/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
/*******************************************************************************
 * @page CoreConcepts_MaxxEffect MaxxEffect
 *
 * @ref MaxxEffect_t or simply **effect** - is a Waves algorithms handler, that
 *   contains:
 *   @ref MaxxEffect_InternalData_Parameters,
 *   @ref MaxxEffect_InternalData_Coefficients,
 *   @ref MaxxEffect_InternalData_States,
 *   @ref MaxxEffect_InternalData_Meters, etc. required during
 *   effect life-cycle. It is caller responsibility to allocate enough memory
 *   for storing effect handler, and to properly initialize it. Refer to the
 *   @ref Initialization page for handler allocation and initialization.
 *
 * @section MaxxEffect_InternalData Data structures
 * @subsection MaxxEffect_InternalData_Parameters Parameters
 *   Data segment with algorithms parameters representing some
 *   configuration available for tuning, e.g. Master Bypass, Mute, Parametric
 *   EQ band frequency, gain etc. *Parameters preset* is an array of parameter
 *   ID and float-point value (id, value) pairs. Might not be included in
 *   MaxxEffect handler. **Updated in the control path.**
 *
 * @subsection MaxxEffect_InternalData_Coefficients Coefficients
 *   Data segment with algorithms coefficients such as filters
 *   difference equations. Computed from parameters and used for processing.
 *   Single coefficient is represented by its ID and the corresponding buffer.
 *   *Coefficients preset* is an array of such pairs.
 *   **Updated in the control path.**
 *
 * @subsection MaxxEffect_InternalData_States States
 *   Data segment with algorithms states.
 *   **Updated in the data path.**
 *
 * @subsection MaxxEffect_InternalData_Meters Meters
 *   Data segment with algorithms meters representing measure of some value
 *   over a period of time, e.g. level, gain reduction, peak, etc.
 *   **Updated in the data path.**
 ******************************************************************************/
#ifndef MAXX_EFFECT_H
#define MAXX_EFFECT_H

/**
 * Waves effect handler.
 *
 * @see @ref CoreConcepts_MaxxEffect
 */
typedef void MaxxEffect_t;


#endif
