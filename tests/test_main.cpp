#include "test_support.h"

void runBlockingQueueTests(test_support::TestSuite& suite);
void runCliTests(test_support::TestSuite& suite);
void runConfigurationTests(test_support::TestSuite& suite);
void runIoTests(test_support::TestSuite& suite);
void runStatusTests(test_support::TestSuite& suite);
void runThreadingTests(test_support::TestSuite& suite);

int main() {
    test_support::TestSuite suite;
    runBlockingQueueTests(suite);
    runCliTests(suite);
    runStatusTests(suite);
    runConfigurationTests(suite);
    runIoTests(suite);
    runThreadingTests(suite);
    return suite.finish();
}
