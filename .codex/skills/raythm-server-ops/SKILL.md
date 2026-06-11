---
name: raythm-server-ops
description: Operational notes for raythm-server. Use when working with the raythm-server repository, server API changes, deployment, production/dev server access, Tailscale SSH access, or verifying the deployed raythm server.
---

# raythm-server Ops

Use this skill when a task touches the raythm server repository, server API, deploy flow, or deployed environment.

## Local Repository

- The server repository is available from the raythm client repo parent directory:
  - Windows path: `C:\Users\rento\CLionProjects\raythm-server`
  - From `C:\Users\rento\CLionProjects\raythm`: `..\raythm-server`
- Git remote:
  - `origin https://github.com/Rofutok112/raythm-Server.git`

## Remote Access

- The deployed server host is reachable over Tailscale as:
  - SSH target: `raythm@raythm-server`
  - Known Tailscale IP used previously: `100.119.194.50`
- From Windows/PowerShell, prefer the Windows OpenSSH binary when needed:
  - `C:\Windows\Sysnative\OpenSSH\ssh.exe raythm@raythm-server`
  - If the host name does not resolve, use `raythm@100.119.194.50`.

## Remote Repository And Deploy

- Remote repo path:
  - `/home/raythm/raythm-server`
- Deployment command used previously:
  - `cd /home/raythm/raythm-server && ./deploy.sh dev`
- Example one-liner:

```powershell
C:\Windows\Sysnative\OpenSSH\ssh.exe raythm@raythm-server "cd /home/raythm/raythm-server && ./deploy.sh dev"
```

## Verification

After deploying, verify the service is healthy before reporting success.

Useful checks:

```powershell
C:\Windows\Sysnative\OpenSSH\ssh.exe raythm@raythm-server "cd /home/raythm/raythm-server && docker compose ps"
C:\Windows\Sysnative\OpenSSH\ssh.exe raythm@raythm-server "curl -fsS http://localhost:3000/health"
```

For auth-protected routes, a `401` can be the expected result. For example, `/users/test/profile` previously returned `401` when unauthenticated.

## Workflow Notes

- Make server code changes in `C:\Users\rento\CLionProjects\raythm-server`, not in the client repository.
- Commit and push server changes from the server repository before deploying.
- Do not assume deployment succeeded just because the SSH command exited; check health and container status.
- Avoid storing secrets in this skill. Use existing environment files and remote server configuration.
