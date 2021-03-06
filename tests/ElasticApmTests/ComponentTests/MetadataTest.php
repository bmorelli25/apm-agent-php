<?php

declare(strict_types=1);

namespace Elastic\Apm\Tests\ComponentTests;

use Elastic\Apm\Impl\Constants;
use Elastic\Apm\Impl\MetadataDiscoverer;
use Elastic\Apm\Tests\ComponentTests\Util\ComponentTestCaseBase;
use Elastic\Apm\Tests\ComponentTests\Util\DataFromAgent;
use Elastic\Apm\Tests\ComponentTests\Util\TestEnvBase;
use Elastic\Apm\Tests\ComponentTests\Util\TestProperties;

final class MetadataTest extends ComponentTestCaseBase
{
    private static function generateDummyMaxKeywordString(): string
    {
        return '[' . str_repeat('V', (Constants::KEYWORD_STRING_MAX_LENGTH - 4) / 2)
               . ','
               . ';'
               . str_repeat('W', (Constants::KEYWORD_STRING_MAX_LENGTH - 4) / 2) . ']';
    }

    public function testDefaultEnvironment(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty'])),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyEnvironment(null, $dataFromAgent);
            }
        );
    }

    public function testCustomEnvironment(): void
    {
        $expected = 'custom service environment 9.8 @CI#!?';
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredEnvironment($expected),
            function (DataFromAgent $dataFromAgent) use ($expected): void {
                TestEnvBase::verifyEnvironment($expected, $dataFromAgent);
            }
        );
    }

    public function testInvalidEnvironmentTooLong(): void
    {
        $validPart = self::generateDummyMaxKeywordString();
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredEnvironment($validPart . '_tail'),
            function (DataFromAgent $dataFromAgent) use ($validPart): void {
                TestEnvBase::verifyEnvironment($validPart, $dataFromAgent);
            }
        );
    }

    public function testDefaultServiceName(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty'])),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyServiceName(MetadataDiscoverer::DEFAULT_SERVICE_NAME, $dataFromAgent);
            }
        );
    }

    public function testCustomServiceName(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredServiceName('custom service name'),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyServiceName('custom service name', $dataFromAgent);
            }
        );
    }

    public function testInvalidServiceNameChars(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredServiceName(
                '1CUSTOM -@- sErvIcE -+- NaMe9'
            ),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyServiceName('1CUSTOM -_- sErvIcE -_- NaMe9', $dataFromAgent);
            }
        );
    }

    public function testInvalidServiceNameTooLong(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredServiceName(
                '[' . str_repeat('A', (Constants::KEYWORD_STRING_MAX_LENGTH - 4) / 2)
                . ','
                . ';'
                . str_repeat('B', (Constants::KEYWORD_STRING_MAX_LENGTH - 4) / 2) . ']' . '_tail'
            ),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyServiceName(
                    '_' . str_repeat('A', Constants::KEYWORD_STRING_MAX_LENGTH / 2 - 2)
                    . '_'
                    . '_'
                    . str_repeat('B', Constants::KEYWORD_STRING_MAX_LENGTH / 2 - 2) . '_',
                    $dataFromAgent
                );
            }
        );
    }

    public function testDefaultServiceVersion(): void
    {
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty'])),
            function (DataFromAgent $dataFromAgent): void {
                TestEnvBase::verifyServiceVersion(null, $dataFromAgent);
            }
        );
    }

    public function testCustomServiceVersion(): void
    {
        $expected = 'v1.5.4-alpha@CI#.!?.';
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredServiceVersion($expected),
            function (DataFromAgent $dataFromAgent) use ($expected): void {
                TestEnvBase::verifyServiceVersion($expected, $dataFromAgent);
            }
        );
    }

    public function testInvalidServiceVersionTooLong(): void
    {
        $validPart = self::generateDummyMaxKeywordString();
        $this->sendRequestToInstrumentedAppAndVerifyDataFromAgentEx(
            (new TestProperties([__CLASS__, 'appCodeEmpty']))->withConfiguredServiceVersion($validPart . '_tail'),
            function (DataFromAgent $dataFromAgent) use ($validPart): void {
                TestEnvBase::verifyServiceVersion($validPart, $dataFromAgent);
            }
        );
    }
}
