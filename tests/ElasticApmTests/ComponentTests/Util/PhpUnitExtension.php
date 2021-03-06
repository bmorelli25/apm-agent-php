<?php

/**
 * PhpUnitExtension is used in phpunit_component_tests.xml
 *
 * @noinspection PhpUnused
 */

declare(strict_types=1);

namespace Elastic\Apm\Tests\ComponentTests\Util;

use Elastic\Apm\Impl\Log\Logger;
use Elastic\Apm\Impl\Util\IdGenerator;
use Elastic\Apm\Tests\Util\TestLogCategory;
use PHPUnit\Runner\AfterIncompleteTestHook;
use PHPUnit\Runner\AfterRiskyTestHook;
use PHPUnit\Runner\AfterSkippedTestHook;
use PHPUnit\Runner\AfterSuccessfulTestHook;
use PHPUnit\Runner\AfterTestErrorHook;
use PHPUnit\Runner\AfterTestFailureHook;
use PHPUnit\Runner\AfterTestHook;
use PHPUnit\Runner\AfterTestWarningHook;
use PHPUnit\Runner\BeforeTestHook;

final class PhpUnitExtension implements
    BeforeTestHook,
    AfterTestHook,
    AfterSuccessfulTestHook,
    AfterTestFailureHook,
    AfterTestErrorHook,
    AfterTestWarningHook,
    AfterSkippedTestHook,
    AfterIncompleteTestHook,
    AfterRiskyTestHook
{
    /** @var Logger */
    private $logger;

    /** @var string */
    public static $testEnvId;

    public function __construct()
    {
        ComponentTestCaseBase::init();

        $this->logger = AmbientContext::loggerFactory()->loggerForClass(
            TestLogCategory::TEST_UTIL,
            __NAMESPACE__,
            __CLASS__,
            __FILE__
        )->addContext('appCodeHostKind', AppCodeHostKind::toString(AmbientContext::config()->appCodeHostKind()));
    }

    public function executeBeforeTest(string $test): void
    {
        self::$testEnvId = IdGenerator::generateId(/* idLengthInBytes */ 16);

        ($loggerProxy = $this->logger->ifDebugLevelEnabled(__LINE__, __FUNCTION__))
        && $loggerProxy->log('Test starting...', ['test' => $test, 'testEnvId' => self::$testEnvId]);
    }

    public function executeAfterTest(string $test, float $time): void
    {
        ($loggerProxy = $this->logger->ifDebugLevelEnabled(__LINE__, __FUNCTION__))
        && $loggerProxy->log('Test finished', ['test' => $test, 'time' => $time, 'testEnvId' => self::$testEnvId]);
    }

    public function executeAfterSuccessfulTest(string $test, float $time): void
    {
        ($loggerProxy = $this->logger->ifDebugLevelEnabled(__LINE__, __FUNCTION__))
        && $loggerProxy->log(
            'Test finished successfully',
            ['test' => $test, 'time' => $time, 'testEnvId' => self::$testEnvId]
        );
    }

    private function testFinishedUnsuccessfully(string $issue, string $test, string $message, float $time): void
    {
        ($loggerProxy = $this->logger->ifDebugLevelEnabled(__LINE__, __FUNCTION__))
        && $loggerProxy->log(
            "Test finished $issue",
            ['test' => $test, 'message' => $message, 'time' => $time, 'testEnvId' => self::$testEnvId]
        );
    }

    public function executeAfterTestFailure(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('with failure', $test, $message, $time);
    }

    public function executeAfterTestError(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('with error', $test, $message, $time);
    }

    public function executeAfterTestWarning(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('with warning', $test, $message, $time);
    }

    public function executeAfterSkippedTest(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('as skipped', $test, $message, $time);
    }

    public function executeAfterIncompleteTest(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('as incomplete', $test, $message, $time);
    }

    public function executeAfterRiskyTest(string $test, string $message, float $time): void
    {
        $this->testFinishedUnsuccessfully('as risky', $test, $message, $time);
    }
}
