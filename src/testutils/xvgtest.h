/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2015, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \libinternal \file
 * \brief
 * Declares function to add the content of an xvg file to a checker.
 *
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 * \author Teemu Murtola <teemu.murtola@gmail.com>
 * \inlibraryapi
 * \ingroup module_testutils
 */
#ifndef GMX_TESTUTILS_XVGTESTS_H
#define GMX_TESTUTILS_XVGTESTS_H

#include <string>

#include "testutils/testasserts.h"
#include "testutils/textblockmatchers.h"

namespace gmx
{

class TextInputStream;

namespace test
{

class TestReferenceChecker;

struct XvgMatchSettings
{
    XvgMatchSettings() : tolerance(defaultRealTolerance()) {}

    FloatingPointTolerance  tolerance;
};

/*! \brief
 * Adds content of xvg file to TestReferenceChecker object.
 *
 * A stream of strings is parsed. The columns
 * are analyzed with a relative tolerance provided by the input.
 * Xmgrace formatting is ignored and only multi-column data is
 * understood.
 *
 * \param[in] input       Object returning the lines of the file/data
 *                        one by one.
 * \param[in,out] checker The checker object.
 * \param[in] settings    Settings to use for matching.
 */
void checkXvgFile(TextInputStream        *input,
                  TestReferenceChecker   *checker,
                  const XvgMatchSettings &settings);

/*! \libinternal \brief
 * Match the contents as an xvg file.
 *
 * \see checkXvgFile()
 *
 * \inlibraryapi
 * \ingroup module_testutils
 */
class XvgMatch : public ITextBlockMatcherSettings
{
    public:
        //! Sets the tolerance for matching data point values.
        XvgMatch &tolerance(const FloatingPointTolerance &tolerance)
        {
            settings_.tolerance = tolerance;
            return *this;
        }

        virtual TextBlockMatcherPointer createMatcher() const;

    private:
        XvgMatchSettings  settings_;
};

} // namespace test

} // namespace gmx

#endif
