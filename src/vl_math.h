/*
 * Vive Libre
 *
 * Copyright (C) 2016 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#pragma once

#include <Eigen/Geometry>
#include <osvr/PluginKit/TrackerInterfaceC.h>

static inline Eigen::Quaterniond switch_coord_order(Eigen::Quaterniond* in) {
    Eigen::Quaterniond q(in->x(), in->y(), in->z(), in->w());
    return q;
}

static inline Eigen::Quaterniond eigen_quaternion_inverse_handedness(Eigen::Quaterniond in) {
    Eigen::Quaterniond q(in.w(), -in.x(), -in.y(), -in.z());
    return q;
}

static inline void print_eigen_quat(const char* label, Eigen::Quaterniond* in) {
    printf("%s: %f %f %f %f\n", label, in->w(), in->x(), in->y(), in->z());
}
