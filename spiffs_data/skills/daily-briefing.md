# Daily Briefing

Compile a personalized daily briefing for the user.

## When to use
When the user asks for a daily briefing, morning update, or "what's new today".
Also useful as a heartbeat/cron task.

## How to use
1. Use get_current_time for today's date
2. Use memory_search to find relevant profile, preference, and task-related nodes
3. Use memory_read_node only for the most relevant nodes you need in detail
4. Use web_search for relevant news based on user interests
5. Compile a concise briefing covering:
   - Date and time
   - Weather (if location known from USER.md)
   - Relevant news/updates based on user interests
   - Any pending tasks from memory
   - Any scheduled cron jobs

## Format
Keep it brief — 5-10 bullet points max. Use the user's preferred language.
