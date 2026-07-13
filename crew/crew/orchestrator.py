"""The phased workflow that drives the crew.

Flow: PLAN -> (checkpoint) -> CODE -> REVIEW -> (checkpoint) -> TEST -> repair loop
-> (checkpoint) -> hand back to the human to commit.

We keep a single ``ClaudeSDKClient`` session for the whole task so context carries
across phases (the coder sees the plan, the tester sees the diff, etc.). Between
phases we pause for human approval — that is what "checkpoints" means in practice.

The SDK is imported lazily so the rest of the package imports without it installed.
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import AsyncContextManager, Callable

from rich.console import Console
from rich.panel import Panel
from rich.rule import Rule

from . import session as session_mod
from .agents import build_agents
from .config import CrewConfig
from .summary import format_run_summary
from .transcript import format_transcript, write_transcript
from .verdict import interpret_test_result

# Builds the async-context-manager client for a run. Injectable so tests can
# drive the workflow without the SDK or a live API key.
ClientFactory = Callable[[CrewConfig, "str | None"], AsyncContextManager]

console = Console()

# A checkpoint asks the human a yes/no question and returns their answer.
Confirm = Callable[[str], bool]


@dataclass
class PhaseResult:
    text: str
    session_id: str | None
    cost_usd: float | None = None


def _extract_cost(message) -> float | None:
    """Pull the per-turn cost off a result message, if the SDK reports one."""
    cost = getattr(message, "total_cost_usd", None)
    if isinstance(cost, (int, float)):
        return float(cost)
    return None


def _extract_text(message) -> str:
    """Pull human-readable text out of an SDK message, defensively.

    Message/block classes differ across SDK versions, so we probe by attribute
    rather than importing concrete types.
    """
    parts: list[str] = []
    content = getattr(message, "content", None)
    if isinstance(content, str):
        return content
    if content:
        for block in content:
            text = getattr(block, "text", None)
            if isinstance(text, str):
                parts.append(text)
    return "".join(parts)


async def _run_phase(client, prompt: str, *, title: str) -> PhaseResult:
    """Send one phase prompt, stream the response, return collected text + session id."""
    console.print(Rule(f"[bold cyan]{title}[/bold cyan]"))
    await client.query(prompt)

    collected: list[str] = []
    session_id: str | None = None
    cost: float | None = None
    async for message in client.receive_response():
        text = _extract_text(message)
        if text:
            console.print(text, end="")
            collected.append(text)
        # ResultMessage (end of turn) carries the session id and cost.
        sid = getattr(message, "session_id", None)
        if sid:
            session_id = sid
        c = _extract_cost(message)
        if c is not None:
            cost = c
    console.print()  # newline after streamed output
    return PhaseResult(text="".join(collected), session_id=session_id, cost_usd=cost)


def _build_options(config: CrewConfig, resume: str | None):
    from claude_agent_sdk import ClaudeAgentOptions

    return ClaudeAgentOptions(
        agents=build_agents(config),
        allowed_tools=["Read", "Write", "Edit", "Bash", "Glob", "Grep", "Agent"],
        # Coder edits are auto-approved; planner/reviewer/tester are read-only by
        # tool scoping, so they cannot edit regardless of this setting.
        permission_mode="acceptEdits",
        max_turns=config.max_turns,
        resume=resume,
    )


def _default_client_factory(config: CrewConfig, resume: str | None):
    """Real SDK client. Imported lazily so the package works without the SDK."""
    from claude_agent_sdk import ClaudeSDKClient

    return ClaudeSDKClient(options=_build_options(config, resume))


async def run_task(
    task: str,
    config: CrewConfig,
    *,
    confirm: Confirm,
    resume_session_id: str | None = None,
    client_factory: ClientFactory | None = None,
    dry_run: bool = False,
) -> session_mod.SessionState:
    """Drive a task through the crew with human checkpoints between phases.

    ``client_factory`` defaults to the real SDK client; tests inject a fake to
    exercise the phase order, checkpoint gating, and repair loop offline.
    ``dry_run`` runs only the planner and stops — a preview with no changes.
    """
    factory = client_factory or _default_client_factory
    state = session_mod.SessionState(session_id=resume_session_id, task=task)

    # Per-phase records, in run order: (title, cost) for the summary and
    # (title, text) for the saved transcript.
    records: list[tuple[str, float | None]] = []
    transcript: list[tuple[str, str]] = []

    async with factory(config, resume_session_id) as client:

        def checkpoint(result: PhaseResult, phase: str) -> bool:
            if result.session_id:
                state.session_id = result.session_id
            # Per-turn costs sum to the task total. (SDK reports cost per query
            # turn; if a future SDK reports it cumulatively this would need a max
            # instead — revisit if the numbers look inflated.)
            if result.cost_usd:
                state.total_cost_usd += result.cost_usd
            state.phase = phase
            session_mod.save(state, config)
            return True

        async def phase(prompt: str, *, title: str, name: str) -> PhaseResult:
            """Run one phase, checkpoint it, and record it for the summary."""
            result = await _run_phase(client, prompt, title=title)
            checkpoint(result, name)
            records.append((title, result.cost_usd))
            transcript.append((title, result.text))
            return result

        # 1. PLAN --------------------------------------------------------------
        await phase(
            f"Use the planner agent to produce an implementation plan for this task:\n\n{task}",
            title="1/4  PLAN",
            name="plan",
        )
        if dry_run:
            console.print(
                "[cyan]Dry run:[/cyan] planned only — no code was written and nothing was changed."
            )
            return state
        if not confirm("Approve this plan and let the coder implement it?"):
            console.print("[yellow]Stopped at the plan checkpoint. Nothing was changed.[/yellow]")
            return state

        # 2. CODE --------------------------------------------------------------
        await phase(
            "Use the coder agent to implement the approved plan exactly. Do not commit or push.",
            title="2/4  CODE",
            name="code",
        )

        # 3. REVIEW ------------------------------------------------------------
        await phase(
            "Use the reviewer agent to review the current working diff "
            "(run `git diff`) for correctness and security issues.",
            title="3/4  REVIEW",
            name="review",
        )
        if confirm("Reviewer done. Apply the reviewer's suggested fixes now?"):
            await phase(
                "Use the coder agent to apply the reviewer's suggested fixes.",
                title="3b/4  APPLY REVIEW FIXES",
                name="review",
            )

        # 4. TEST + bounded repair loop ---------------------------------------
        for attempt in range(1, config.max_fix_rounds + 1):
            test = await phase(
                "Use the tester agent to find and run the project's tests and "
                "linters, then report pass/fail with the key failing output.",
                title=f"4/4  TEST (round {attempt}/{config.max_fix_rounds})",
                name="test",
            )
            verdict = interpret_test_result(test.text)
            if verdict is True:
                console.print("[green]Tests look green.[/green]")
                break
            if verdict is None:
                console.print(
                    "[yellow]Couldn't determine the test result from the tester's "
                    "report; treating it as not-yet-green.[/yellow]"
                )
            if attempt == config.max_fix_rounds:
                console.print(
                    f"[yellow]Reached the {config.max_fix_rounds}-round fix limit; "
                    "leaving the diff for you to inspect.[/yellow]"
                )
                break
            if not confirm(f"Tests failing. Let the coder attempt fix round {attempt + 1}?"):
                break
            await phase(
                "Use the coder agent to fix the failing tests the tester reported. "
                "Change only what's needed to make them pass.",
                title=f"4b/4  FIX (round {attempt})",
                name="code",
            )

        # 5. COMMIT is a human decision. We stop here on purpose.
        state.phase = "done"
        session_mod.save(state, config)
        transcript_path = write_transcript(
            config.state_dir() / "runs",
            state.session_id or "latest",
            format_transcript(task, transcript),
        )
        console.print("\n" + format_run_summary(records, state.total_cost_usd))
        console.print(f"[dim]Transcript saved to {transcript_path}[/dim]")
        console.print(
            Panel.fit(
                "Crew finished. Review the diff with [bold]git diff[/bold], then commit when "
                "you're happy.\nThe crew never commits or pushes on its own.",
                title="Done",
                border_style="green",
            )
        )
    return state


def run_task_sync(
    task: str,
    config: CrewConfig,
    *,
    confirm: Confirm,
    resume_session_id: str | None = None,
    dry_run: bool = False,
) -> session_mod.SessionState:
    """Blocking wrapper around :func:`run_task` for the CLI."""
    return asyncio.run(
        run_task(
            task,
            config,
            confirm=confirm,
            resume_session_id=resume_session_id,
            dry_run=dry_run,
        )
    )
