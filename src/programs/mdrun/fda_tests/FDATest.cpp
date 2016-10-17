/*
 * FDATest.cpp
 *
 *  Created on: Sep 1, 2014
 *      Author: Bernd Doser, HITS gGmbH
 */

#include <iostream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "gromacs/options/filenameoption.h"
#include "gromacs/utility/futil.h"
#include "gromacs/utility/path.h"
#include "gromacs/utility/real.h"
#include "gromacs/gmxpreprocess/grompp.h"
#include "programs/mdrun/mdrun_main.h"
#include "testutils/cmdlinetest.h"
#include "testutils/integrationtests.h"
#include "testutils/TextSplitter.h"
#include "testutils/LogicallyErrorComparer.h"

namespace {

struct TestDataStructure
{
	TestDataStructure(
		std::string const& testDirectory,
		std::string const& atomFileExtension,
		std::string const& residueFileExtension
	)
	  : testDirectory(testDirectory),
		atomFileExtension(atomFileExtension),
		residueFileExtension(residueFileExtension)
	{}

    std::string testDirectory;
    std::string atomFileExtension;
    std::string residueFileExtension;
};

} // namespace anonymous

//! Test fixture for FDA
class FDATest : public ::testing::WithParamInterface<TestDataStructure>,
                public ::gmx::test::IntegrationTestFixture
{};

//! Test body for FDA
TEST_P(FDATest, Basic)
{
	std::string cwd = gmx::Path::getWorkingDirectory();
	std::string dataPath = std::string(fileManager_.getInputDataDirectory()) + "/data";
	std::string testPath = fileManager_.getTemporaryFilePath("/" + GetParam().testDirectory);

	std::string cmd = "mkdir -p " + testPath;
	ASSERT_FALSE(system(cmd.c_str()));

	cmd = "cp -r " + dataPath + "/" + GetParam().testDirectory + "/* " + testPath;
	ASSERT_FALSE(system(cmd.c_str()));

	gmx_chdir(testPath.c_str());

	std::string atomFilename = "fda." + GetParam().atomFileExtension;
	std::string atomOption = "-" + GetParam().atomFileExtension;
	std::string atomReference = atomFilename + ".ref";
	std::string residueFilename = "fda." + GetParam().residueFileExtension;
	std::string residueOption = "-" + GetParam().residueFileExtension;
	std::string residueReference = residueFilename + ".ref";

    ::gmx::test::CommandLine callRerun;
    callRerun.append("gmx_fda mdrun");
    callRerun.addOption("-deffnm", "rerun");
    callRerun.addOption("-s", "topol.tpr");
    callRerun.addOption("-rerun", "traj.trr");
    callRerun.addOption("-nt", "1");
    callRerun.addOption("-pfn", "index.ndx");
    callRerun.addOption("-pfi", "fda.pfi");
    if (!GetParam().atomFileExtension.empty()) callRerun.addOption(atomOption.c_str(), atomFilename.c_str());
    if (!GetParam().residueFileExtension.empty()) callRerun.addOption(residueOption.c_str(), residueFilename.c_str());

    ASSERT_FALSE(gmx_mdrun(callRerun.argc(), callRerun.argv()));

    const double error_factor = 1.0e4;
    const bool weight_by_magnitude = false;
    const bool ignore_sign = true;

    LogicallyEqualComparer<weight_by_magnitude,ignore_sign> comparer(error_factor);

    // Compare files
    if (!GetParam().atomFileExtension.empty()) EXPECT_TRUE((compare(TextSplitter(atomFilename), TextSplitter(atomReference), comparer)));
    if (!GetParam().residueFileExtension.empty()) EXPECT_TRUE((compare(TextSplitter(residueFilename), TextSplitter(residueReference), comparer)));

	gmx_chdir(cwd.c_str());
}

INSTANTIATE_TEST_CASE_P(AllFDATests, FDATest, ::testing::Values(
    TestDataStructure("alagly_pairwise_forces_scalar", "pfa", "pfr"),
    TestDataStructure("alagly_pairwise_forces_scalar_atom_based", "pfa", ""),
    TestDataStructure("alagly_pairwise_forces_scalar_no_residue_based", "pfa", ""),
    TestDataStructure("alagly_pairwise_forces_scalar_detailed_no_residue_based", "pfa", ""),
	TestDataStructure("alagly_pairwise_forces_vector", "pfa", "pfr"),
	TestDataStructure("alagly_punctual_stress", "psa", "psr"),
	TestDataStructure("alagly_pairwise_forces_scalar_detailed_nonbonded", "pfa", "pfr"),
	TestDataStructure("alagly_pairwise_forces_vector_detailed_nonbonded", "pfa", "pfr"),
    TestDataStructure("alagly_verlet_summed_scalar", "pfa", "pfr"),
    TestDataStructure("alagly_group_excl", "pfa", "pfr"),
    TestDataStructure("alagly_group_excl_uncomplete_cgs", "pfa", "pfr"),
    TestDataStructure("alagly_pairwise_forces_scalar_all", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl1", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl2", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl3", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl4", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl5", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_excl6", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_group_bonded_excl1", "pfa", "pfr"),
    TestDataStructure("glycine_trimer_virial_stress", "vsa", ""),
    TestDataStructure("glycine_trimer_virial_stress_von_mises", "vma", "")
));
