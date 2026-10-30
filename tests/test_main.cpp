#include "test_support.h"

void runBlockingQueueTests(test_support::TestSuite& suite);
void runCanParserTests(test_support::TestSuite& suite);
void runCliTests(test_support::TestSuite& suite);
void runConfigurationTests(test_support::TestSuite& suite);
void runIoTests(test_support::TestSuite& suite);
void runSignalTests(test_support::TestSuite& suite);
void runSocketCanTests(test_support::TestSuite& suite);
void runStatusTests(test_support::TestSuite& suite);
void runThreadingTests(test_support::TestSuite& suite);

int main() {
    test_support::TestSuite suite;
    runBlockingQueueTests(suite);
    runCanParserTests(suite);
    runCliTests(suite);
    runStatusTests(suite);
    runConfigurationTests(suite);
    runIoTests(suite);
    runThreadingTests(suite);
    runSignalTests(suite);
    runSocketCanTests(suite);
    return suite.finish();
}
