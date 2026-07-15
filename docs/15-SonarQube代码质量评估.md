# SonarQube 代码质量评估

## 1. 功能目标

本项目新增 SonarQube 代码质量评估入口，用于在 DevOps 流程中对 C++ 代码进行静态扫描，辅助发现代码坏味道、复杂度风险、重复代码和潜在缺陷。

这项能力对应“质量前移”：在代码合并和交付之前，先通过自动化工具发现问题，减少后期测试和答辩演示中的低级风险。

## 2. 新增文件

| 文件 | 作用 |
|------|------|
| `sonar-project.properties` | SonarScanner 项目配置，声明项目名、源码目录、测试目录和排除路径 |
| `.gitlab-ci.yml` | 新增 `quality` 阶段和 `sonarqube` job，在 GitLab CI 中执行扫描 |
| `.gitignore` | 忽略 `.scannerwork/` 和 `sonar-report/` 等扫描临时产物 |

## 3. 本地使用方式

先准备一个可访问的 SonarQube 服务，例如本地 Docker：

```powershell
docker run -d --name sonarqube -p 9000:9000 sonarqube:community
```

启动后访问：

```text
http://localhost:9000
```

默认账号通常为：

```text
admin / admin
```

在 SonarQube Web 页面创建项目并生成 token，然后在项目根目录运行：

```powershell
sonar-scanner `
  -Dsonar.host.url=http://localhost:9000 `
  -Dsonar.token=你的Token
```

如果需要指定项目 Key：

```powershell
sonar-scanner `
  -Dsonar.host.url=http://localhost:9000 `
  -Dsonar.token=你的Token `
  -Dsonar.projectKey=cpp-ai-agent
```

## 4. GitLab CI 使用方式

GitLab CI 中新增了 `sonarqube` job，位于 `quality` 阶段。该 job 只有在配置了以下 CI/CD Variables 时才会运行：

| 变量 | 必填 | 说明 |
|------|------|------|
| `SONAR_HOST_URL` | 是 | SonarQube 服务地址，例如 `http://localhost:9000` 或内网服务器地址 |
| `SONAR_TOKEN` | 是 | SonarQube 生成的项目 token |
| `SONAR_PROJECT_KEY` | 否 | 项目 Key，未设置时默认使用 `cpp-ai-agent` |

配置位置：

```text
GitLab 项目 -> Settings -> CI/CD -> Variables
```

注意：

- 不要把 SonarQube token 写入 `.gitlab-ci.yml`。
- `SONAR_TOKEN` 应配置为受保护或隐藏变量。
- Runner 机器需要安装 `sonar-scanner`，并保证命令在 `PATH` 中可用。
- 如果没有配置 `SONAR_HOST_URL` 和 `SONAR_TOKEN`，`sonarqube` job 会自动跳过，不影响现有构建和测试。

## 5. 当前扫描范围

当前 `sonar-project.properties` 中配置：

```properties
sonar.sources=src
sonar.tests=tests
sonar.sourceEncoding=UTF-8
```

排除内容包括：

- `build/**`
- `vcpkg_installed/**`
- `logs/**`
- `docs/**`
- `第一周周报/**`
- `config/**`
- `examples/**`

这样可以避免把构建产物、依赖目录、日志和文档纳入代码质量统计。

## 6. 答辩讲法

可以这样介绍：

> 我们在 GitLab CI 中增加了 SonarQube 质量评估阶段。构建和测试通过后，如果项目配置了 SonarQube 地址和 token，Runner 会自动执行 `sonar-scanner`，把源码目录和测试目录的静态分析结果上传到 SonarQube 平台。这样项目不仅能编译和测试，还能对代码质量进行量化管理。

重点说明三点：

1. **质量前移**：问题在合并和交付前被发现。
2. **自动化集成**：通过 GitLab CI 的 `quality` 阶段执行。
3. **安全配置**：token 使用 GitLab Variables，不写入仓库。

## 7. 已知边界

- 当前实现提供 SonarScanner 接入入口，是否能分析 C++ 规则取决于 SonarQube 服务端插件和版本。
- 如果 Runner 未安装 `sonar-scanner`，job 会明确报错提示安装。
- 如果没有配置 SonarQube 变量，job 会跳过，不影响普通 CI。
