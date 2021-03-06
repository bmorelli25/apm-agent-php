<?php

/** @noinspection PhpUndefinedClassInspection */

declare(strict_types=1);

namespace Elastic\Apm\Impl;

use Closure;
use Elastic\Apm\ExecutionSegmentInterface;
use Elastic\Apm\Impl\Util\DbgUtil;
use Elastic\Apm\SpanInterface;

/**
 * Code in this file is part of implementation internals and thus it is not covered by the backward compatibility.
 *
 * @internal
 */
abstract class NoopExecutionSegment implements ExecutionSegmentInterface
{
    /** @var string */
    public const ID = '0000000000000000';

    /** @var string */
    public const TRACE_ID = '00000000000000000000000000000000';

    /** @var string */
    public const NAME = 'NO-OP';

    /** @var string */
    public const TYPE = 'noop';

    /** @inheritDoc */
    public function getTimestamp(): float
    {
        return 0.0;
    }

    /** @inheritDoc */
    public function getDuration(): float
    {
        return 0.0;
    }

    /** @inheritDoc */
    public function getId(): string
    {
        return self::ID;
    }

    /** @inheritDoc */
    public function getName(): string
    {
        return self::NAME;
    }

    /** @inheritDoc */
    public function setName(string $name): void
    {
    }

    /** @inheritDoc */
    public function getTraceId(): string
    {
        return self::TRACE_ID;
    }

    /** @inheritDoc */
    public function getType(): string
    {
        return self::TYPE;
    }

    /** @inheritDoc */
    public function setType(string $type): void
    {
    }

    /** @inheritDoc */
    public function beginChildSpan(
        string $name,
        string $type,
        ?string $subtype = null,
        ?string $action = null,
        ?float $timestamp = null
    ): SpanInterface {
        return NoopSpan::singletonInstance();
    }

    /** @inheritDoc */
    public function captureChildSpan(
        string $name,
        string $type,
        Closure $callback,
        ?string $subtype = null,
        ?string $action = null,
        ?float $timestamp = null
    ) {
        return $callback(NoopSpan::singletonInstance());
    }

    /** @inheritDoc */
    public function end(?float $duration = null): void
    {
    }

    /** @inheritDoc */
    public function hasEnded(): bool
    {
        return true;
    }

    /** @inheritDoc */
    public function setLabel(string $key, $value): void
    {
    }

    /** @inheritDoc */
    public function getLabels(): array
    {
        return [];
    }

    public function discard(): void
    {
    }

    public function __toString(): string
    {
        return DbgUtil::fqToShortClassName(get_class());
    }
}
