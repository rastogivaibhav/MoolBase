# MoolPay hosted demo

MoolPay is a hackathon demo showing an autonomous payment agent that calls MoolBase as a separate service before Stripe execution.

Architecture: `MoolPay agent -> MoolBase service -> resolved/contested receipt -> policy gate -> Stripe test/demo`.

The MoolBase service embeds the unchanged MoolBase database and HypoKosh/DWM runtime through the native customer-showcase lifecycle adapter. MoolPay never treats an LLM response as payment authority: only `status=resolved` with the PAY hypothesis selected can open the Stripe gate. Any `open`, `provisionally_resolved`, `contested`, or HOLD selection blocks execution.

The hosted demo is safe by default: Stripe uses a no-network demo result unless an `sk_test_...` key is explicitly configured; live Stripe keys are refused. The MoolBase `/v1/evaluate` endpoint is protected by a shared bearer token between the two services.

This folder is demo/integration code only. It does not modify MoolBase core behavior or claim production readiness.
