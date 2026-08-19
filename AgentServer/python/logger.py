import logging
import sys
from pathlib import Path

class CustomLogger(logging.Logger):
    def __init__(self, name: str):
        super().__init__(name)
        self.setLevel(logging.DEBUG)
        self._file_handler = None
        self._console_handler = logging.StreamHandler(sys.stdout)
        self._console_handler.setLevel(logging.INFO)
        self._console_handler.setFormatter(self._ColorFormatter())
        self.addHandler(self._console_handler)

    class _ColorFormatter(logging.Formatter):
        COLORS = {"INFO": "\033[32minfo\033[0m", "WARNING": "\033[33mwarn\033[0m", "ERROR": "\033[31merr\033[0m"}
        def format(self, record):
            level_tag = self.COLORS.get(record.levelname, record.levelname.lower())
            # 使用 self.formatTime 获取标准格式时间，并通过 record 传递给输出
            time_str = self.formatTime(record, "%Y-%m-%d %H:%M:%S")
            return f"[{time_str}] {level_tag}:{record.getMessage()}"

    def set_log_dir(self, file_path: str | Path, level: str = "DEBUG"):
        """设置日志输出文件路径。如果多次调用，会安全切换文件。"""
        path = Path(file_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        if self._file_handler:
            self.removeHandler(self._file_handler)
            self._file_handler.close()
            
        self._file_handler = logging.FileHandler(path, mode="w", encoding="utf-8")
        self._file_handler.setLevel(getattr(logging, level.upper()))
        
        file_formatter = logging.Formatter(
            "{asctime} | {levelname:<5} | {filename}:{lineno} | {message}", 
            style="{"
        )
        self._file_handler.setFormatter(file_formatter)
        self.addHandler(self._file_handler)
        self.info(f"本地日志文件已绑定至: {path.resolve()}")

    def set_console_level(self, level: str = "DEBUG"):
        """动态修改控制台等级 (如 'DEBUG', 'INFO')"""
        self._console_handler.setLevel(getattr(logging, level.upper()))
        self.info(f"控制台日志等级已更改为: {level}")


logging.setLoggerClass(CustomLogger)


from typing import cast
logger = cast(CustomLogger, logging.getLogger(__name__))