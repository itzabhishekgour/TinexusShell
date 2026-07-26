# Tinexus Platform — Search Ranking Engine Specification

> **Document:** 21_SEARCH_RANKING.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 06_COMPONENT_DESIGN.md

---

## Table of Contents

1. [Ranking Engine Overview](#1-ranking-engine-overview)
2. [Scoring Architecture & Pipeline](#2-scoring-architecture--pipeline)
3. [Base Category Scoring Weights](#3-base-category-scoring-weights)
4. [Fuzzy Match Algorithm (Trigram + Levenshtein)](#4-fuzzy-match-algorithm-trigram--levenshtein)
5. [Usage Frequency & Recency Bonus Functions](#5-usage-frequency--recency-bonus-functions)
6. [Pinned & Secondary Action Heuristics](#6-pinned--secondary-action-heuristics)
7. [AI Ranking Hook Protocol (`?` Query Prefix)](#7-ai-ranking-hook-protocol--query-prefix)

---

## 1. Ranking Engine Overview

The Search Ranking Engine inside `tinexus-searchd` converts raw query strings into an ordered list of high-relevance search items in < 5 milliseconds.

It turns Ctrl+K from a simple app launcher into an **intent-prediction interface**.

---

## 2. Scoring Architecture & Pipeline

```
User Query ("fire")
      │
      ▼
┌─────────────────────────┐
│ Provider Fan-out        │ (Parallel execution across App, File, Calc, System, Plugins)
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Raw Results Aggregator  │ (Collects candidate SearchResult items)
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Trigram + Levenshtein   │ Score = MatchScore(query, item.title) * BaseWeight
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Recency & Frequency     │ FinalScore = Score * (1.0 + FrequencyFactor) + RecencyBonus + PinnedBonus
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Sorting & Top-N Cutoff  │ Sort descending by FinalScore → Deliver Top 6 to Launcher UI
└─────────────────────────┘
```

---

## 3. Base Category Scoring Weights

| Category | Base Weight | Rationale |
|---|---|---|
| **Exact Match App** | 1000 | User typed exact binary/app name |
| **System Actions** (Lock, Shutdown) | 900 | High intent safety actions |
| **Inline Calculator Result** | 850 | User typed valid math expression |
| **Fuzzy App Match** | 700–890 | App name partially matched |
| **Recent Files** | 500 | User opening recent work |
| **Clipboard History** | 400 | User searching past text copy |
| **Third-Party Plugins** | 300–600 | Plugin-provided items |

---

## 4. Fuzzy Match Algorithm (Trigram + Levenshtein)

### 4.1 Trigram Overlap Calculation

$$\text{TrigramScore}(Q, T) = \frac{2 \times |S(Q) \cap S(T)|}{|S(Q)| + |S(T)|}$$

where $S(X)$ is the set of 3-character slices of string $X$.

### 4.2 Prefix Match Advantage

If item title **starts with** query $Q$:
$\text{PrefixBonus} = +200 \text{ points}$

---

## 5. Usage Frequency & Recency Bonus Functions

### 5.1 Recency Decay Function

Recency bonus decays exponentially over time since last launch:

$$R(t) = R_{\text{max}} \times e^{-\lambda t}$$

- $R_{\text{max}} = 400 \text{ points}$
- $\lambda = \frac{\ln(2)}{168} \approx 0.00412 \text{ (Half-life of 7 days / 168 hours)}$
- $t$: Hours since last launch.

### 5.2 Frequency Factor

$$F(n) = \min\left(1.5, 1.0 + 0.1 \times \ln(1 + n)\right)$$

where $n$ is total lifetime launches of this item.

---

## 6. Pinned & Secondary Action Heuristics

- **Pinned Items:** Items pinned by user receive immutable $+300$ points bonus.
- **Top Result Auto-Selection:** If $\text{FinalScore}(\text{Item}_0) - \text{FinalScore}(\text{Item}_1) > 200$, Item 0 is visually highlighted as primary default activation.

---

## 7. AI Ranking Hook Protocol (`?` Query Prefix)

When a query starts with `?` (e.g., `? open my Python project from last week`):
1. Normal providers are bypassed or deprioritized.
2. Query is routed to `AIProvider` slot.
3. Local LLM / NLP intent classifier emits candidate targets with confidence scores.
4. Engine ranks candidates based on AI confidence $\times 1000$.

---

*Document End: 21_SEARCH_RANKING.md*
