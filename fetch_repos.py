import json
import subprocess
from pathlib import Path


def load_repos():
    root = Path(__file__).resolve().parent
    with open(root / "repos.json", encoding="utf-8") as file:
        return root, json.load(file)


def fetch_repo(root, repo):
    path = root / repo["path"]
    if not (path / ".git").exists():
        subprocess.run(["git", "clone", repo["url"], str(path)], check=True)
    else:
        subprocess.run(["git", "-C", str(path), "fetch", "--tags", "--prune"], check=True)
    subprocess.run(["git", "-C", str(path), "checkout", repo["branch"]], check=True)


if __name__ == "__main__":
    project_root, repositories = load_repos()
    for repository in repositories:
        fetch_repo(project_root, repository)

