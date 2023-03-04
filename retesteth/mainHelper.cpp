#include <AllTestNames.h>
#include <EthChecks.h>
#include <ExitHandler.h>
#include <Options.h>
#include <TestHelper.h>
#include <TestOutputHelper.h>
#include <mainHelper.h>
#include <iostream>
#include <sstream>
#include <thread>

#include <retesteth/testSuites/DifficultyTest.h>
#include <retesteth/testSuites/TransactionTest.h>
#include <retesteth/testSuites/blockchain/BlockchainTests.h>
#include <retesteth/testSuites/statetests/StateTests.h>

#include <libdataobj/ConvertFile.h>
#include <libdevcore/CommonIO.h>

using namespace std;
using namespace test::debug;
using namespace boost::unit_test;

namespace
{
static std::ostringstream strCout;
std::streambuf* oldCoutStreamBuf;
std::streambuf* oldCerrStreamBuf;
std::string const c_sDynamicTestSuiteName = "customTestSuite";
std::vector<const char*> c_cleanDynamicChars;
const char** c_argv2 = nullptr;

void runCustomTestFile()
{
    test::Options const& opt = test::Options::get();
    auto runSuite = [&opt](test::TestSuite* _suite) {
        if (opt.singleTestFile.initialized())
        {
            boost::filesystem::path file(opt.singleTestFile);
            _suite->runTestWithoutFiller(file);
        }
    };

    if (opt.rCurrentTestSuite.find("GeneralStateTests") != std::string::npos)
    {
        test::StateTestSuite suite;
        runSuite(&suite);
    }
    else if (opt.rCurrentTestSuite.find("BlockchainTests") != std::string::npos)
    {
        if (opt.rCurrentTestSuite.find("InvalidBlocks") != std::string::npos)
        {
            test::BlockchainTestInvalidSuite suite;
            runSuite(&suite);
        }
        else if (opt.rCurrentTestSuite.find("ValidBlocks") != std::string::npos)
        {
            test::BlockchainTestValidSuite suite;
            runSuite(&suite);
        }
        else if (opt.rCurrentTestSuite.find("TransitionTests") != std::string::npos)
        {
            test::BlockchainTestTransitionSuite suite;
            runSuite(&suite);
        }
        else
        {
            test::BlockchainTestInvalidSuite suite;
            runSuite(&suite);
        }
    }
    else if (opt.rCurrentTestSuite.find("DifficultyTests") != std::string::npos)
    {
        test::DifficultyTestSuite suite;
        runSuite(&suite);
    }
    else if (opt.rCurrentTestSuite.find("TransactionTests") != std::string::npos)
    {
        test::TransactionTestSuite suite;
        runSuite(&suite);
    }
    else
    {
        string sMinusTArg = opt.rCurrentTestSuite;
        std::cerr << "Error: '" + sMinusTArg + "' suite not found! \n";
        test::main::printTestSuiteSuggestions(sMinusTArg);
    }
}

void customTestSuite()
{
    test::Options const& opt = test::Options::get();
    if (opt.singleTestFile.initialized())
        runCustomTestFile();
}
}  // namespace


namespace test::main
{
void initializeOptions(int _argc, const char* _argv[])
{
    try
    {
        test::Options::get(_argc, _argv);
    }
    catch (test::Options::InvalidOption const& e)
    {
        std::cerr << e.what() << "\n";
        exit(1);
    }
}

void expandUnitTestsArg(int _argc, const char* _argv[])
{
    for (int i = 0; i < _argc; i++)
    {
        // replace test suite to custom tests
        std::string arg = std::string{_argv[i]};
        if (arg == "-t" && i + 1 < _argc && string(_argv[i + 1]) == "UnitTests")
        {
            _argv[i + 1] =
                "LLLCSuite,SOLCSuite,DataObjectTestSuite,EthObjectsSuite,OptionsSuite,TestHelperSuite,ExpectSectionSuite,"
                "trDataCompileSuite,StructTest,MemoryLeak";
            break;
        }
    }
}

void makeSingleTestFileSuite(int _argc, const char* _argv[])
{
    test::Options const& opt = test::Options::get();
    if (opt.singleTestFile.initialized())
    {
        bool testSuiteFound = false;
        for (int i = 0; i < _argc; i++)
        {
            // replace test suite to custom tests
            std::string arg = std::string{_argv[i]};
            if (arg == "-t" && i + 1 < _argc)
            {
                testSuiteFound = true;
                _argv[i + 1] = (char*)c_sDynamicTestSuiteName.c_str();
                break;
            }
        }

        // BOOST ERROR could not be used here because boost main is not initialized
        if (!testSuiteFound)
        {
            if (opt.singleTestFile.initialized())
            {
                std::cerr << "testfile <file>  requires a test suite to be set -t <TestSuite>\n";
                exit(-1);
            }
        }

        // add custom test suite
        test_suite* ts1 = BOOST_TEST_SUITE(c_sDynamicTestSuiteName);
        ts1->add(BOOST_TEST_CASE(&customTestSuite));
        framework::master_test_suite().add(ts1);
    }
}

void travisOut(std::atomic_bool* _stopTravisOut)
{
    int tickCounter = 0;
    while (!*_stopTravisOut)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++tickCounter;
        if (tickCounter % 10 == 0)
            std::cout << ".\n" << std::flush;  // Output dot every 10s.
        if (ExitHandler::receivedExitSignal())
            break;
    }
}
void timeoutThread(std::atomic_bool* _stopTimeout)
{
    uint tickCounter = 0;
    const uint C_MAX_TESTEXEC_TIMEOUT = 36000;
    while (!*_stopTimeout)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++tickCounter;
        if (tickCounter > C_MAX_TESTEXEC_TIMEOUT)
        {
            test::TestOutputHelper::get().setCurrentTestInfo(test::TestInfo("Timeout"));
            ETH_FAIL_MESSAGE("Test execution timeout reached! " + test::fto_string(C_MAX_TESTEXEC_TIMEOUT) + "sec");
        }
        if (ExitHandler::receivedExitSignal())
            break;
    }
}
void printTestSuiteSuggestions(string const& _sMinusTArg)
{
    auto const testList = test::levenshteinDistance(_sMinusTArg, c_allTestNames);
    std::cerr << "Did you mean: \n";
    for (auto const& element : testList)
        std::cerr << "-t " << element << "\n";
}

bool checkTestSuiteIsKnown(int argc, const char* argv[], string sMinusTArg)
{
    // unit_test_main delete this option from _argv
    for (int i = 0; i < argc; i++)  // find -t boost arg
    {
        std::string const arg = std::string{argv[i]};
        if (arg == "-t" && i + 1 < argc)
        {
            sMinusTArg = std::string{argv[i + 1]};
            break;
        }
    }

    if (!sMinusTArg.empty())
    {
        bool requestSuiteNotFound = false;
        std::vector<std::string> requestedSuites = test::explode(sMinusTArg, ',');
        for (auto const& suite : requestedSuites)
        {
            if (!test::inArray(c_allTestNames, suite))
            {
                requestSuiteNotFound = true;
                sMinusTArg = suite;
                break;
            }
        }

        // Print suggestions of a test case if test suite not found
        if (requestSuiteNotFound && sMinusTArg != c_sDynamicTestSuiteName)
        {
            std::cerr << "Error: '" + sMinusTArg + "' suite not found! \n";
            printTestSuiteSuggestions(sMinusTArg);
            return false;
        }
    }
    return true;
}

void disableOutput()
{
    oldCoutStreamBuf = std::cout.rdbuf();
    oldCerrStreamBuf = std::cerr.rdbuf();
    std::cout.rdbuf(strCout.rdbuf());
    std::cerr.rdbuf(strCout.rdbuf());
}
void enableOutput()
{
    std::cout.rdbuf(oldCoutStreamBuf);
    std::cerr.rdbuf(oldCerrStreamBuf);
}

int runTheBoostTests(int _argc, const char* _argv[])
{
    int result = 0;
    try
    {
        test::Options const& opt = test::Options::get();
        boost::filesystem::path const testPath = test::getTestPath();
        ETH_DC_MESSAGE(DC::STATS, string("Running tests using path: ") + testPath.c_str());

        auto fakeInit = [](int, char* []) -> boost::unit_test::test_suite* { return nullptr; };
        if (opt.vmtrace || !opt.travisOutThread || opt.getvectors)
        {
            if (opt.getvectors)
                disableOutput();

            std::atomic_bool stopTimeout{false};
            std::thread timeout(timeoutThread, &stopTimeout);
            // Do not use travis '.' output thread if debug is defined
            result = unit_test_main(fakeInit, _argc, const_cast<char**>(_argv));
            stopTimeout = true;
            timeout.join();

            if (opt.getvectors)
            {
                enableOutput();
                test::TestOutputHelper::get().printTestVectors();
                disableOutput();
            }
        }
        else
        {
            // Initialize travis '.' output thread for log activity
            std::atomic_bool stopTravisOut{false};
            std::thread outputThread(travisOut, &stopTravisOut);
            result = unit_test_main(fakeInit, _argc, const_cast<char**>(_argv));
            stopTravisOut = true;
            outputThread.join();
        }
    }
    catch (test::UpwardsException const& _ex)
    {
        ETH_STDERROR_MESSAGE(string("Error: ") + _ex.what());
    }
    return result;
}

void lookForUnregisteredTestFolders()
{
    typedef vector<string> vectorString;
    std::vector<string>& cheatAllTestNames = const_cast<vectorString&>(c_allTestNames);
    DynamicTestsBoost(cheatAllTestNames);
}

void cleanMemory()
{
    DynamicTestsBoostClean();
    for (size_t i = 0; i < c_cleanDynamicChars.size(); i++)
        delete [] c_cleanDynamicChars[i];
    if (c_argv2 != nullptr)
        delete [] c_argv2;
}

string getTestType(string const& _filename)
{
    string type = "GeneralStateTests";
    if (boost::filesystem::exists(_filename))
    {
        string s = dev::contentsString(_filename);
        spDataObject res = dataobject::ConvertJsoncppStringToData(s);

        auto isBlockChainTest = [](DataObject const& _el)
        {
            return (_el.getKey() == "blocks") ? true : false;
        };
        auto isStateTest = [](DataObject const& _el)
        {
            return (_el.getKey() == "env") ? true : false;
        };
        if (res->performSearch(isBlockChainTest))
            return "BlockchainTests";
        if (res->performSearch(isStateTest))
            return "GeneralStateTests";
    }
    return type;
}

// Preprocess the args
const char** preprocessOptions(int& _argc, const char* _argv[])
{
    // if options has a /path/to/file.json | file.yml and it is not a --testfile option
    // make it a --testfile option and attempt to get the type of test and insert -t <suite>
    short tArgument = -1;
    short insertTestFile = -1;
    string testFilename;
    for (short i = 1; i < _argc; i++)
    {
        std::string arg = std::string{_argv[i]};
        bool isFile = (arg.find(".json") != string::npos || arg.find(".yml") != string::npos);
        if (isFile && std::string{_argv[i - 1]} != "--testfile")
        {
            insertTestFile = i;
            testFilename = arg;
        }
        else if (arg == "-t")
            tArgument = i;
    }

    if (insertTestFile == -1)
        return _argv;

    size_t inserts = 0;
    if (tArgument == -1)
        inserts += 3;
    if (insertTestFile != -1)
        inserts +=1;

    size_t j = 0;
    const char** argv2 = new const char*[_argc + inserts];
    argv2[j++] = _argv[0];
    if (tArgument == -1)
    {
        argv2[j++] = "-t";

        string const testType = getTestType(testFilename);
        const std::string::size_type size = testType.size();
        char *buffer = new char[size + 1];   //we need extra char for NUL
        memcpy(buffer, testType.c_str(), size + 1);
        c_cleanDynamicChars.emplace_back(buffer);

        argv2[j++] = buffer;
        argv2[j++] = "--";
    }

    for (short i = 1; i < _argc; i++)
    {
        if (i == insertTestFile)
            argv2[j++] = "--testfile";
        argv2[j++] = _argv[i];
    }

    _argc += inserts;
    c_argv2 = argv2;
    return argv2;
}


}  // namespace test::main
