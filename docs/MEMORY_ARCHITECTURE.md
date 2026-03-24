# 当前记忆架构说明

本文档描述 BareBrain 当前代码里已经实现的记忆架构，重点说明“记忆存在哪里、怎么进入上下文、如何写入和删除、失败时会发生什么”。

## 1. 边界定义

当前系统里和“记忆”相关的内容分成三类：

1. 提示词基底
`/spiffs/config/SOUL.md` 和 `/spiffs/config/USER.md` 会在每轮构造 system prompt 时直接拼进去。它们是静态配置，不属于可检索、可建索引的长期记忆。

2. 会话短期记忆
每个聊天会话都会把用户消息和最终回复追加到 `sessions/*.jsonl`，作为下一轮对话的历史上下文。这一层是“短期上下文记忆”。

3. 长期索引记忆
真正的长期记忆存放在 `/sdcard/memory/` 下，由索引文件、节点正文、元数据、异步建库队列共同组成。这一层支持搜索、展开关联、按节点读取和删除。

一句话概括：BareBrain 现在不是单一 `MEMORY.md` 模式，而是“会话历史 + 索引化长期记忆”的双层结构。

## 2. 总体结构

```text
用户消息
  -> agent_loop
     -> 读取 session 历史
     -> 构造 system prompt
        -> 拼接 SOUL.md / USER.md
        -> 拼接长期记忆摘要目录
     -> 调用 LLM
     -> 需要时调用记忆工具

长期写入路径:
memory_upsert_note
  -> /sdcard/memory/inbox/*.json
  -> memory_worker 轮询队列
  -> memory_model 生成摘要/标签/关联/候选 match_id
  -> 写入 nodes/*.md + meta/*.json
  -> 更新 index.json

长期读取路径:
system prompt 只注入摘要目录
  -> memory_search 找节点
  -> memory_read_node 读正文
  -> memory_expand_links 看关联节点
```

## 3. 分层说明

### 3.1 会话短期记忆

- 存储位置：`<data_base>/sessions/tg_<chat_id>.jsonl`
- `data_base` 优先走 `/sdcard`，否则回落到 `/spiffs`
- 每行是一个 JSON 对象，字段为 `role`、`content`、`ts`
- 当前只保存两类内容：本轮用户输入、最终 assistant 文本回复
- 工具调用中间过程不会写入 session
- 读取时会把最近 `BRN_SESSION_MAX_MSGS = 20` 条消息装成 JSON 数组送给 LLM

这层的职责是保证“同一个会话里能接上文”，不是做长期知识沉淀，也不会自动参与长期索引。

### 3.2 长期索引记忆

长期记忆由 `brn_memory_node_t` 表示，核心字段包括：

- `id`：节点 ID，形如 `mem_xxxxxxxx`
- `kind`：记忆类型，例如 `profile`、`doc`、`skill`、`session`、`note`
- `title`：节点标题
- `summary`：供目录和搜索展示的摘要
- `detail_path`：正文文件路径
- `tags[]`：标签
- `link_ids[]`：关联节点 ID
- `score_hint`：基础排序分
- `updated_at`：更新时间

它的设计重点是“摘要与正文分离”：

- `index.json` 只保留轻量目录信息，适合装进内存和 prompt
- `nodes/<id>.md` 保存正文，按需读取
- `meta/<id>.json` 保存建库过程附带的结构化元信息

### 3.3 异步建库层

长期记忆写入不是同步直接改索引，而是先入队再异步处理：

- 新素材先写到 `inbox/<queue_id>.json`
- 后台 `memory_worker` 每 5 秒轮询一次队列
- 成功后删除 inbox 项
- 失败后移动到 `failed/`

这样做把“用户当前对话响应”和“长期记忆建索引”解耦开了，避免在主对话链路里同步等待一次额外的 LLM 摘要请求。

## 4. 存储布局

长期记忆只放在 SD 卡：

| 路径 | 作用 |
|------|------|
| `/sdcard/memory/index.json` | 长期记忆目录索引 |
| `/sdcard/memory/nodes/<id>.md` | 节点正文 |
| `/sdcard/memory/meta/<id>.json` | 标签、关联、来源、解析结果 |
| `/sdcard/memory/inbox/<id>.json` | 等待建库的原始素材 |
| `/sdcard/memory/failed/<id>.json` | 建库失败项 |

需要注意两点：

1. 长期索引记忆没有回落到 SPIFFS
如果 SD 卡没有挂载成功，系统仍能启动、对话、保存 session，但长期索引记忆不会可用。

2. 会话历史和长期记忆走的是不同策略
会话历史通过 `storage_get_data_base()` 选择 `/sdcard` 或 `/spiffs`；长期记忆由 `memory_store` 固定使用 `/sdcard/memory/`。

## 5. 启动与初始化顺序

启动时相关顺序大致是：

1. `storage_init()`
先挂载 SPIFFS，再尝试挂载 SD，并准备 `/sdcard/memory`、`/sdcard/sessions`、`/sdcard/docs`。

2. `memory_store_init()`
准备长期记忆目录结构。如果 SD 未挂载，会明确打日志，但不会伪造可用状态。

3. `memory_index_init()`
从 `/sdcard/memory/index.json` 读入内存中的 `s_nodes[]`。

4. `session_mgr_init()`
准备会话历史管理。

5. `memory_model_init()`
加载专门给记忆建库使用的模型配置。

6. `memory_worker_init()` 与 `memory_worker_start()`
启动后台建库任务。

这说明当前架构是“启动时把长期索引加载进 RAM，运行时用内存目录服务搜索和 prompt 摘要”。

## 6. 读取链路

### 6.1 进入 system prompt 的只有摘要目录

`context_builder` 每轮会把长期记忆摘要拼到 system prompt 的 `Memory Directory` 段落中，但只放摘要，不放正文。

当前限制：

- 最多注入 `BRN_MEMORY_PROMPT_LIMIT = 8` 条节点
- 顺序由 `score_hint` 和 `updated_at` 排序
- 不会根据当前用户问题做动态检索后再注入

所以这更像“常驻顶层目录”，不是“按 query 实时召回的 RAG”。

### 6.2 按需读取的工具化访问

Agent 需要更精确信息时，走记忆工具：

- `memory_search`
对内存中的目录做过滤和排序，支持 `query`、`kind`、`tag`。

- `memory_read_node`
根据 `detail_path` 读取节点正文。

- `memory_expand_links`
顺着 `link_ids` 取出关联节点摘要。

- `memory_reindex_status`
查看待处理数、失败数、已建索引数、最近错误和当前使用的记忆模型。

当前搜索不是 embedding 检索，而是轻量文本匹配加打分：

- `id` 命中加分
- `title` 命中加分最高
- `summary` 和 `tags` 命中也会加分
- `kind`、`tag` 可做硬过滤
- 同分时优先 `updated_at` 更新更近的节点

## 7. 写入链路

### 7.1 入队

长期记忆新增或更新通过 `memory_upsert_note` 触发：

- 必填：`title`、`content`
- 可选：`kind`、`tags`、`source`

如果 agent 没传 `source`，`agent_loop` 会自动补成 `channel:chat_id`，方便追溯来源。

### 7.2 后台建库

后台 worker 处理单个 inbox 项时，会做这些步骤：

1. 读取原始 JSON
拿到 `kind`、`title`、`content`、`tags`、`source`。

2. 构造候选目录
把当前索引前若干条节点整理成 catalog 字符串，提供给记忆模型做比对。

3. 调用记忆模型生成元数据
模型必须返回 JSON，键固定为：
`title`、`summary`、`tags`、`link_ids`、`match_id`

4. 解析“新建还是复用”
如果 `match_id` 指向一个已存在节点，就复用该节点 ID，表示本次写入是在更新已有长期记忆；否则生成新的 `mem_xxxxxxxx`。

5. 落盘
- 正文写入 `nodes/<id>.md`
- 节点写入或覆盖 `index.json`
- 解析结果写入 `meta/<id>.json`

### 7.3 写入语义

当前长期写入是“覆盖式 upsert”：

- 命中新节点时新增
- 命中已有 `match_id` 时复用同一个 `id`
- `tags` 会合并去重
- `link_ids` 以模型返回结果为准重建
- `summary`、`title`、`updated_at` 等会被新结果覆盖

这意味着系统具备“同一长期事实被重新总结、而不是无限重复堆积”的基础能力，但是否复用完全依赖记忆模型给出的 `match_id`。

## 8. 删除链路

删除通过 `memory_delete_node` 完成，顺序是：

1. 先确认节点存在
2. 删除 `nodes/<id>.md`
3. 删除 `meta/<id>.json`
4. 更新 `index.json`
5. 把其他节点里指向该节点的 `link_ids` 清掉

这里有一个很重要的实现语义：

- 删除不是事务性的
- 如果正文和元数据文件已经删掉，但 `index.json` 更新失败，工具会返回明确错误
- 系统不会静默掩盖这种不一致

这个行为符合当前项目的 debug-first 策略：失败直接暴露，方便定位根因。

## 9. 记忆模型配置

长期记忆建库可以使用一套独立于主对话 LLM 的配置：

- `memory_api_key`
- `memory_model`
- `memory_provider`
- `memory_base_url`

如果没有完整单独配置，会按字段回退到主 LLM 配置，因此当前状态里会显示 `using_fallback`。

这套模型当前只负责：

- 生成标题
- 生成摘要
- 提取标签
- 提取关联节点
- 判断是否应该更新已有节点

它不直接参与主对话回复生成。

## 10. 一致性与并发语义

当前实现里有几条重要语义：

- 长期索引在 RAM 中维护一份数组缓存，访问受互斥锁保护
- `index.json` 写入采用临时文件再 `rename` 的方式，降低半写入风险
- worker 是单任务串行消费 inbox，不做并行建库
- 长期记忆写入不是完整事务
正文、索引、元数据分三步写，任一步失败都可能留下部分已落盘状态

换句话说，当前架构更偏“轻量、可调试、资源可控”，而不是“强事务、强一致”的数据库式实现。

## 11. 当前限制

截至当前代码，长期记忆系统的边界很明确：

- 最大节点数 `BRN_MEMORY_MAX_NODES = 64`
- 每节点最多 6 个标签、8 条关联
- prompt 常驻摘要最多 8 条
- 提供给记忆模型的候选 catalog 最多 12 条
- 默认搜索结果最多 6 条
- 没有 embedding、向量库或语义召回
- 没有自动把每轮 session 持久化为长期记忆
- 没有跨节点复杂图查询，只有一跳 `link_ids`
- 长期记忆强依赖 SD 卡挂载成功

## 12. 当前架构的本质

BareBrain 当前的记忆架构，本质上是一个面向嵌入式设备资源约束设计的三段式系统：

- 用 `sessions/*.jsonl` 解决短期连续对话
- 用 `/sdcard/memory/index.json + nodes + meta` 解决长期知识沉淀
- 用 `inbox -> worker -> memory model` 解决异步摘要、打标签、建关联和去重更新

它不是通用数据库，也不是标准 RAG 系统，而是“可持久、可检索、可显式调试”的轻量记忆子系统。
