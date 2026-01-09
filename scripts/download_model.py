#!/usr/bin/env python3
"""
YOLO模型下载脚本
支持YOLOv8、YOLOv10和YOLOv11模型：
1. 使用ultralytics库自动导出ONNX模型（推荐）
2. 从网络下载预编译的ONNX模型
"""

import os
import sys
import urllib.request
import shutil
import subprocess
from pathlib import Path

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent
MODELS_DIR = PROJECT_ROOT / "models"

# YOLO模型信息（优先推荐YOLOv8，兼容性更好）
MODELS = {
    # YOLOv8 模型（推荐，使用 opset 17，兼容性最好）
    "yolov8n": {
        "name": "YOLOv8n (Nano)",
        "size": "~6MB",
        "pt_model": "yolov8n.pt",
        "opset": 17,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n.onnx",
        "description": "最小最快的模型，适合移动设备和边缘设备（推荐）"
    },
    "yolov8s": {
        "name": "YOLOv8s (Small)",
        "size": "~22MB",
        "pt_model": "yolov8s.pt",
        "opset": 17,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8s.onnx",
        "description": "小型模型，平衡速度和精度（推荐）"
    },
    "yolov8m": {
        "name": "YOLOv8m (Medium)",
        "size": "~52MB",
        "pt_model": "yolov8m.pt",
        "opset": 17,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8m.onnx",
        "description": "中型模型，更好的精度（推荐）"
    },
    "yolov8l": {
        "name": "YOLOv8l (Large)",
        "size": "~87MB",
        "pt_model": "yolov8l.pt",
        "opset": 17,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8l.onnx",
        "description": "大型模型，高精度（推荐）"
    },
    "yolov8x": {
        "name": "YOLOv8x (Extra Large)",
        "size": "~136MB",
        "pt_model": "yolov8x.pt",
        "opset": 17,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8x.onnx",
        "description": "超大型模型，最高精度（推荐）"
    },
    # YOLOv10 模型（需要 opset 21+ 的 ONNX Runtime）
    "yolov10n": {
        "name": "YOLOv10n (Nano)",
        "size": "~6MB",
        "pt_model": "yolov10n.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10n.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10n.onnx",
        "description": "最小最快的模型，适合移动设备和边缘设备（需要 opset 21+）"
    },
    "yolov10s": {
        "name": "YOLOv10s (Small)",
        "size": "~22MB",
        "pt_model": "yolov10s.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10s.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10s.onnx",
        "description": "小型模型，平衡速度和精度（需要 opset 21+）"
    },
    "yolov10m": {
        "name": "YOLOv10m (Medium)",
        "size": "~52MB",
        "pt_model": "yolov10m.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10m.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10m.onnx",
        "description": "中型模型，更好的精度（需要 opset 21+）"
    },
    "yolov10b": {
        "name": "YOLOv10b (Large)",
        "size": "~87MB",
        "pt_model": "yolov10b.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10b.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10b.onnx",
        "description": "大型模型，高精度（需要 opset 21+）"
    },
    "yolov10l": {
        "name": "YOLOv10l (Extra Large)",
        "size": "~142MB",
        "pt_model": "yolov10l.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10l.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10l.onnx",
        "description": "超大型模型，最高精度（需要 opset 21+）"
    },
    "yolov10x": {
        "name": "YOLOv10x (Extra Extra Large)",
        "size": "~219MB",
        "pt_model": "yolov10x.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov10x.onnx",
        "backup_url": "https://github.com/THU-MIG/yolov10/releases/download/v1.0/yolov10x.onnx",
        "description": "最大模型，最高精度（速度较慢，需要 opset 21+）"
    },
    # YOLOv11 模型（最新版本，需要 opset 21+ 的 ONNX Runtime）
    "yolov11n": {
        "name": "YOLOv11n (Nano)",
        "size": "~6MB",
        "pt_model": "yolo11n.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n.onnx",
        "description": "最小最快的模型，适合移动设备和边缘设备（最新版本，需要 opset 21+）"
    },
    "yolov11s": {
        "name": "YOLOv11s (Small)",
        "size": "~22MB",
        "pt_model": "yolo11s.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11s.onnx",
        "description": "小型模型，平衡速度和精度（最新版本，需要 opset 21+）"
    },
    "yolov11m": {
        "name": "YOLOv11m (Medium)",
        "size": "~52MB",
        "pt_model": "yolo11m.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11m.onnx",
        "description": "中型模型，更好的精度（最新版本，需要 opset 21+）"
    },
    "yolov11l": {
        "name": "YOLOv11l (Large)",
        "size": "~87MB",
        "pt_model": "yolo11l.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11l.onnx",
        "description": "大型模型，高精度（最新版本，需要 opset 21+）"
    },
    "yolov11x": {
        "name": "YOLOv11x (Extra Large)",
        "size": "~136MB",
        "pt_model": "yolo11x.pt",
        "opset": 22,
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11x.onnx",
        "description": "超大型模型，最高精度（最新版本，需要 opset 21+）"
    }
}


def check_ultralytics_installed():
    """检查ultralytics是否已安装"""
    # 首先检查虚拟环境
    venv_path = PROJECT_ROOT / ".venv"
    if venv_path.exists():
        import platform
        if platform.system() == "Windows":
            python_exe = venv_path / "Scripts" / "python.exe"
        else:
            python_exe = venv_path / "bin" / "python"
        
        if python_exe.exists():
            try:
                result = subprocess.run([str(python_exe), "-c", "import ultralytics"], 
                                      capture_output=True, 
                                      timeout=5)
                if result.returncode == 0:
                    return True
            except:
                pass
    
    # 检查系统环境
    try:
        import ultralytics
        return True
    except ImportError:
        return False


def check_uv_installed():
    """检查uv是否已安装"""
    try:
        result = subprocess.run(["uv", "--version"], 
                              capture_output=True, 
                              text=True, 
                              timeout=5)
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
        return False


def install_uv():
    """安装uv包管理器"""
    print("\n检测到未安装uv包管理器")
    print("uv是一个快速的Python包管理器，可以解决pip的代理问题")
    response = input("是否现在安装uv? (Y/n): ").strip().lower()
    if response == 'n':
        return False
    
    import platform
    system = platform.system().lower()
    
    print("\n正在安装uv...")
    try:
        if system == "windows":
            # Windows: 使用PowerShell安装
            print("使用PowerShell安装uv...")
            install_cmd = [
                "powershell",
                "-ExecutionPolicy", "ByPass",
                "-Command",
                "irm https://astral.sh/uv/install.ps1 | iex"
            ]
            subprocess.check_call(install_cmd, timeout=120)
        else:
            # Linux/Mac: 使用curl安装
            print("使用curl安装uv...")
            install_cmd = [
                "sh", "-c",
                "curl -LsSf https://astral.sh/uv/install.sh | sh"
            ]
            subprocess.check_call(install_cmd, timeout=120)
        
        # 验证安装
        print("\n验证uv安装...")
        if check_uv_installed():
            print("✓ uv安装成功!")
            # 提示用户可能需要重新启动终端或刷新PATH
            print("\n注意: 如果后续命令提示找不到uv，请:")
            print("  - 重新启动终端，或")
            print("  - 运行: $env:PATH = [System.Environment]::GetEnvironmentVariable(\"Path\",\"Machine\") + \";\" + [System.Environment]::GetEnvironmentVariable(\"Path\",\"User\") (Windows PowerShell)")
            return True
        else:
            print("⚠ uv安装完成，但可能需要重新启动终端才能使用")
            print("请重新启动终端后再次运行此脚本")
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ 安装超时")
        print("\n请手动安装uv:")
        if system == "windows":
            print("  powershell -ExecutionPolicy ByPass -c \"irm https://astral.sh/uv/install.ps1 | iex\"")
        else:
            print("  curl -LsSf https://astral.sh/uv/install.sh | sh")
        return False
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"✗ 自动安装失败: {e}")
        print("\n请手动安装uv:")
        if system == "windows":
            print("  powershell -ExecutionPolicy ByPass -c \"irm https://astral.sh/uv/install.ps1 | iex\"")
        else:
            print("  curl -LsSf https://astral.sh/uv/install.sh | sh")
        print("\n安装完成后，请重新运行此脚本")
        return False


def install_ultralytics():
    """安装ultralytics库，优先使用uv"""
    print("\n检测到未安装ultralytics库")
    print("ultralytics是导出ONNX模型的最佳方式")
    
    # 检查是否安装了uv
    use_uv = check_uv_installed()
    
    if use_uv:
        print("\n检测到已安装uv，将使用uv安装ultralytics（推荐）")
        response = input("是否现在安装? (Y/n): ").strip().lower()
        if response == 'n':
            return False
        
        print("\n正在使用uv安装ultralytics...")
        try:
            # 检查是否在虚拟环境中，如果不在则创建
            venv_path = PROJECT_ROOT / ".venv"
            if not venv_path.exists():
                print("创建虚拟环境...")
                subprocess.check_call(["uv", "venv", str(venv_path)], 
                                    timeout=60)
                print("✓ 虚拟环境创建成功")
            
            # 在虚拟环境中安装ultralytics
            # Windows使用.venv\Scripts\python.exe，Linux/Mac使用.venv/bin/python
            import platform
            if platform.system() == "Windows":
                python_exe = venv_path / "Scripts" / "python.exe"
            else:
                python_exe = venv_path / "bin" / "python"
            
            # 使用uv在虚拟环境中安装
            subprocess.check_call(["uv", "pip", "install", 
                                  "--python", str(python_exe),
                                  "ultralytics"], 
                                timeout=300)
            print("✓ ultralytics安装成功!")
            print(f"  安装位置: {venv_path}")
            return True
        except subprocess.TimeoutExpired:
            print("✗ 安装超时")
            return False
        except subprocess.CalledProcessError as e:
            print(f"✗ uv安装失败: {e}")
            print("\n尝试使用pip安装...")
            # 降级到pip
            use_uv = False
        except FileNotFoundError:
            print("✗ 未找到uv命令")
            print("\n尝试使用pip安装...")
            use_uv = False
    
    if not use_uv:
        # 使用pip安装
        print("\n使用pip安装ultralytics...")
        response = input("是否现在安装? (Y/n): ").strip().lower()
        if response == 'n':
            return False
        
        # 检查虚拟环境
        venv_path = PROJECT_ROOT / ".venv"
        python_exe = None
        
        if venv_path.exists():
            import platform
            if platform.system() == "Windows":
                python_exe = venv_path / "Scripts" / "python.exe"
            else:
                python_exe = venv_path / "bin" / "python"
            
            if python_exe.exists():
                print(f"\n检测到虚拟环境: {venv_path}")
                print("将在虚拟环境中安装ultralytics")
                python_exe = str(python_exe)
            else:
                python_exe = None
        else:
            # 询问是否创建虚拟环境
            print("\n未检测到虚拟环境")
            print("建议在虚拟环境中安装，避免污染系统环境")
            response = input("是否创建虚拟环境并在其中安装? (Y/n): ").strip().lower()
            if response != 'n':
                print("\n正在创建虚拟环境...")
                try:
                    import platform
                    subprocess.check_call([sys.executable, "-m", "venv", str(venv_path)], 
                                        timeout=60)
                    print("✓ 虚拟环境创建成功")
                    
                    if platform.system() == "Windows":
                        python_exe = venv_path / "Scripts" / "python.exe"
                    else:
                        python_exe = venv_path / "bin" / "python"
                    
                    if python_exe.exists():
                        python_exe = str(python_exe)
                        print(f"虚拟环境位置: {venv_path}")
                    else:
                        print("⚠ 虚拟环境创建成功，但未找到Python可执行文件")
                        python_exe = None
                except subprocess.TimeoutExpired:
                    print("✗ 创建虚拟环境超时")
                    python_exe = None
                except subprocess.CalledProcessError as e:
                    print(f"✗ 创建虚拟环境失败: {e}")
                    print("将在系统环境中安装")
                    python_exe = None
        
        # 询问是否先安装uv
        if not python_exe:
            print("\n提示: 如果pip安装失败（如代理问题），建议使用uv:")
            print("  Windows: powershell -ExecutionPolicy ByPass -c \"irm https://astral.sh/uv/install.ps1 | iex\"")
            print("  Linux/Mac: curl -LsSf https://astral.sh/uv/install.sh | sh")
            response = input("是否先安装uv? (y/N): ").strip().lower()
            if response == 'y':
                if install_uv():
                    return install_ultralytics()  # 递归调用，现在应该能用uv了
                return False
        
        try:
            # 使用虚拟环境或系统环境的Python安装
            if python_exe:
                print(f"\n正在使用虚拟环境安装ultralytics...")
                subprocess.check_call([python_exe, "-m", "pip", "install", "ultralytics"], 
                                    timeout=300)
                print("✓ ultralytics安装成功!")
                print(f"  安装位置: {venv_path}")
            else:
                print(f"\n正在使用系统环境安装ultralytics...")
                subprocess.check_call([sys.executable, "-m", "pip", "install", "ultralytics"], 
                                    timeout=300)
                print("✓ ultralytics安装成功!")
            return True
        except subprocess.TimeoutExpired:
            print("✗ 安装超时")
            return False
        except subprocess.CalledProcessError as e:
            print(f"✗ pip安装失败: {e}")
            print("\n建议:")
            print("1. 检查网络连接和代理设置")
            print("2. 使用uv安装: uv pip install ultralytics")
            print("3. 或手动安装: pip install ultralytics --proxy <your-proxy>")
            return False


def export_with_ultralytics(model_key, dest_path):
    """使用ultralytics库导出ONNX模型"""
    if model_key not in MODELS:
        print(f"错误: 未知的模型 '{model_key}'")
        return False
    
    model_info = MODELS[model_key]
    opset = model_info.get("opset", 17)  # 默认使用 opset 17
    
    if not check_ultralytics_installed():
        print("\n" + "="*60)
        print("安装ultralytics库")
        print("="*60)
        if not install_ultralytics():
            return False
    
    # 优先使用虚拟环境中的ultralytics
    venv_path = PROJECT_ROOT / ".venv"
    python_exe = None
    
    if venv_path.exists():
        import platform
        if platform.system() == "Windows":
            python_exe = venv_path / "Scripts" / "python.exe"
        else:
            python_exe = venv_path / "bin" / "python"
        
        if python_exe.exists():
            # 使用虚拟环境的Python运行导出
            print(f"\n使用虚拟环境: {venv_path}")
            try:
                from ultralytics import YOLO
            except ImportError:
                # 如果当前环境没有，使用虚拟环境的Python
                python_exe = str(python_exe)
        else:
            python_exe = None
    
    try:
        if python_exe and Path(python_exe).exists():
            # 使用虚拟环境Python执行导出脚本
            export_code = f"""
import sys
from pathlib import Path
from ultralytics import YOLO
import shutil

model_key = '{model_key}'
dest_path = Path(r'{dest_path}')
opset = {opset}

MODELS = {{
    'yolov8n': 'yolov8n.pt',
    'yolov8s': 'yolov8s.pt',
    'yolov8m': 'yolov8m.pt',
    'yolov8l': 'yolov8l.pt',
    'yolov8x': 'yolov8x.pt',
    'yolov10n': 'yolov10n.pt',
    'yolov10s': 'yolov10s.pt',
    'yolov10m': 'yolov10m.pt',
    'yolov10b': 'yolov10b.pt',
    'yolov10l': 'yolov10l.pt',
    'yolov10x': 'yolov10x.pt',
    'yolov11n': 'yolo11n.pt',
    'yolov11s': 'yolo11s.pt',
    'yolov11m': 'yolo11m.pt',
    'yolov11l': 'yolo11l.pt',
    'yolov11x': 'yolo11x.pt',
}}

pt_model = MODELS.get(model_key, 'yolov8n.pt')
print(f'加载模型: {{pt_model}}')
model = YOLO(pt_model)
print(f'正在导出为ONNX格式 (opset {{opset}})...')
result = model.export(format='onnx', imgsz=640, simplify=True, opset=opset)

# 查找导出的文件
exported_file = Path(f"{{pt_model.replace('.pt', '.onnx')}}")
if not exported_file.exists():
    current_dir = Path.cwd()
    for onnx_file in current_dir.glob("*.onnx"):
        if pt_model.replace('.pt', '') in onnx_file.name:
            exported_file = onnx_file
            break

if exported_file.exists():
    if exported_file != dest_path:
        shutil.move(str(exported_file), str(dest_path))
    print(f'[OK] 模型导出成功: {{dest_path}}')
else:
    print('[FAIL] 未找到导出的ONNX文件')
    sys.exit(1)
"""
            import tempfile
            # 移除Unicode字符，避免Windows GBK编码问题
            export_code_safe = export_code.replace('✓', '[OK]').replace('✗', '[FAIL]')
            with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False, encoding='utf-8') as f:
                f.write(export_code_safe)
                temp_script = f.name
            
            try:
                result = subprocess.run([str(python_exe), temp_script], 
                                      cwd=str(PROJECT_ROOT),
                                      timeout=600,
                                      capture_output=False)
                Path(temp_script).unlink()
                if result.returncode == 0:
                    return True
                else:
                    return False
            except Exception as e:
                if Path(temp_script).exists():
                    Path(temp_script).unlink()
                print(f"虚拟环境执行失败: {e}")
                # 降级到当前环境
                python_exe = None
        
        # 使用当前环境的Python
        if not python_exe:
            from ultralytics import YOLO
        
        pt_model = model_info["pt_model"]
        
        print(f"\n正在使用ultralytics导出 {model_info['name']}...")
        print(f"加载模型: {pt_model}")
        print(f"使用 opset {opset}...")
        
        # 加载模型（会自动下载PyTorch模型）
        model = YOLO(pt_model)
        
        print("正在导出为ONNX格式...")
        # 导出为ONNX（会在当前目录生成文件）
        result = model.export(format='onnx', imgsz=640, simplify=True, opset=opset)
        
        # 查找导出的文件
        # ultralytics会在当前目录或模型所在目录生成ONNX文件
        exported_file = None
        
        # 方法1: 检查result返回的路径
        if hasattr(result, 'path'):
            exported_file = Path(result.path)
        elif isinstance(result, str):
            exported_file = Path(result)
        elif isinstance(result, Path):
            exported_file = result
        
        # 方法2: 在当前目录查找
        if not exported_file or not exported_file.exists():
            exported_file = Path(f"{pt_model.replace('.pt', '.onnx')}")
        
        # 方法3: 在模型文件所在目录查找
        if not exported_file.exists():
            # 检查ultralytics的模型缓存目录
            user_models_dir = Path.home() / ".ultralytics" / "models"
            potential_file = user_models_dir / f"{pt_model.replace('.pt', '.onnx')}"
            if potential_file.exists():
                exported_file = potential_file
        
        # 方法4: 在当前工作目录搜索
        if not exported_file.exists():
            current_dir = Path.cwd()
            for onnx_file in current_dir.glob("*.onnx"):
                if pt_model.replace('.pt', '') in onnx_file.name:
                    exported_file = onnx_file
                    break
        
        if exported_file and exported_file.exists():
            # 移动到目标位置
            if exported_file != dest_path:
                shutil.move(str(exported_file), str(dest_path))
            print(f"✓ 模型导出成功!")
            return True
        else:
            print("✗ 未找到导出的ONNX文件")
            print("提示: 导出的文件可能在当前工作目录，请手动查找")
            return False
            
    except Exception as e:
        print(f"✗ 导出失败: {e}")
        import traceback
        traceback.print_exc()
        return False


def download_file(url, dest_path, description):
    """下载文件并显示进度"""
    print(f"\n正在下载 {description}...")
    print(f"URL: {url}")
    print(f"保存到: {dest_path}")
    
    def show_progress(block_num, block_size, total_size):
        if total_size > 0:
            downloaded = block_num * block_size
            percent = min(downloaded * 100 / total_size, 100)
            bar_length = 50
            filled = int(bar_length * downloaded / total_size)
            bar = '=' * filled + '-' * (bar_length - filled)
            size_mb = downloaded / (1024 * 1024)
            total_mb = total_size / (1024 * 1024)
            print(f'\r[{bar}] {percent:.1f}% ({size_mb:.1f}/{total_mb:.1f} MB)', end='', flush=True)
        else:
            print(f'\r已下载: {block_num * block_size / (1024 * 1024):.1f} MB', end='', flush=True)
    
    try:
        urllib.request.urlretrieve(url, dest_path, show_progress)
        print("\n下载完成!")
        return True
    except Exception as e:
        print(f"\n下载失败: {e}")
        return False


def list_models():
    """列出所有可用的模型"""
    print("\n可用的YOLO模型:")
    print("="*80)
    print("\n【推荐】YOLOv8 模型（兼容性最好，opset 17）:")
    print("-" * 80)
    for key, model in MODELS.items():
        if key.startswith("yolov8"):
            print(f"  {model['name']} ({key})")
            print(f"    大小: {model['size']}")
            print(f"    描述: {model['description']}")
            print()
    
    print("\nYOLOv10 模型（需要 opset 21+ 的 ONNX Runtime）:")
    print("-" * 80)
    for key, model in MODELS.items():
        if key.startswith("yolov10"):
            print(f"  {model['name']} ({key})")
            print(f"    大小: {model['size']}")
            print(f"    描述: {model['description']}")
            print()
    
    print("\nYOLOv11 模型（最新版本，需要 opset 21+ 的 ONNX Runtime）:")
    print("-" * 80)
    for key, model in MODELS.items():
        if key.startswith("yolov11"):
            print(f"  {model['name']} ({key})")
            print(f"    大小: {model['size']}")
            print(f"    描述: {model['description']}")
            print()


def download_model(model_key):
    """下载指定的模型"""
    if model_key not in MODELS:
        print(f"错误: 未知的模型 '{model_key}'")
        print("\n可用的模型:")
        for key in MODELS.keys():
            print(f"  - {key}")
        return False
    
    model_info = MODELS[model_key]
    model_name = model_info["name"]
    model_file = f"{model_key}.onnx"
    dest_path = MODELS_DIR / model_file
    
    # 检查文件是否已存在
    if dest_path.exists():
        file_size = dest_path.stat().st_size / (1024 * 1024)
        print(f"\n模型文件已存在: {dest_path}")
        print(f"文件大小: {file_size:.1f} MB")
        response = input("是否重新下载? (y/N): ").strip().lower()
        if response != 'y':
            print("跳过下载")
            return True
        dest_path.unlink()
    
    # 确保目录存在
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    
    # 下载模型
    print(f"\n{'='*60}")
    print(f"获取 {model_name}")
    print(f"{'='*60}")
    
    # 方法1: 尝试使用ultralytics导出（最可靠）
    print("\n方法1: 使用ultralytics库导出ONNX模型（推荐）")
    response = input("是否使用此方法? (Y/n): ").strip().lower()
    
    if response != 'n':
        if export_with_ultralytics(model_key, dest_path):
            file_size = dest_path.stat().st_size / (1024 * 1024)
            print(f"\n✓ 模型导出成功!")
            print(f"  文件: {dest_path}")
            print(f"  大小: {file_size:.1f} MB")
            return True
        else:
            print("\nultralytics导出失败，尝试网络下载...")
    
    # 方法2: 从网络下载
    print("\n方法2: 从网络下载预编译的ONNX模型")
    url = model_info["url"]
    success = download_file(url, dest_path, model_name)
    
    if not success and "backup_url" in model_info:
        print(f"\n主URL失败，尝试备用URL...")
        success = download_file(model_info["backup_url"], dest_path, model_name)
    
    if success:
        file_size = dest_path.stat().st_size / (1024 * 1024)
        print(f"\n✓ 模型下载成功!")
        print(f"  文件: {dest_path}")
        print(f"  大小: {file_size:.1f} MB")
        return True
    else:
        print(f"\n✗ 所有下载方法都失败了")
        print("\n建议:")
        print("1. 安装ultralytics库: pip install ultralytics")
        print("2. 使用Python手动导出:")
        opset = model_info.get("opset", 17)
        print(f"   from ultralytics import YOLO")
        print(f"   model = YOLO('{model_info.get('pt_model', model_key + '.pt')}')")
        print(f"   model.export(format='onnx', imgsz=640, opset={opset})")
        if dest_path.exists():
            dest_path.unlink()
        return False


def main():
    """主函数"""
    print("="*60)
    print("YOLO C++ 项目 - 模型下载脚本")
    print("="*60)
    print("\n提示: 推荐使用 YOLOv8 模型（兼容性最好）")
    print("      YOLOv10/YOLOv11 模型需要 opset 21+ 的 ONNX Runtime")
    print()
    
    # 解析命令行参数
    if len(sys.argv) > 1:
        model_key = sys.argv[1].lower()
        if model_key == "list":
            list_models()
            return 0
        elif model_key == "all":
            # 下载所有模型
            print("\n将下载所有模型，这可能需要一些时间...")
            response = input("继续? (y/N): ").strip().lower()
            if response != 'y':
                print("取消操作")
                return 0
            
            success_count = 0
            for key in MODELS.keys():
                if download_model(key):
                    success_count += 1
                print()  # 空行分隔
            
            print("="*60)
            print(f"完成: {success_count}/{len(MODELS)} 个模型下载成功")
            print("="*60)
            return 0 if success_count == len(MODELS) else 1
        else:
            # 下载指定模型
            return 0 if download_model(model_key) else 1
    else:
        # 交互式选择
        list_models()
        print("\n请选择要下载的模型:")
        print("  输入模型名称 (如: yolov8n, yolov10n, yolov11n 等)")
        print("  或输入 'all' 下载所有模型")
        print("  或输入 'list' 查看模型列表")
        print()
        
        while True:
            choice = input("请输入选择: ").strip().lower()
            
            if not choice:
                continue
            
            if choice == "list":
                list_models()
                continue
            
            if choice == "all":
                print("\n将下载所有模型，这可能需要一些时间...")
                response = input("继续? (y/N): ").strip().lower()
                if response != 'y':
                    print("取消操作")
                    return 0
                
                success_count = 0
                for key in MODELS.keys():
                    if download_model(key):
                        success_count += 1
                    print()  # 空行分隔
                
                print("="*60)
                print(f"完成: {success_count}/{len(MODELS)} 个模型下载成功")
                print("="*60)
                return 0 if success_count == len(MODELS) else 1
            
            if choice in MODELS:
                return 0 if download_model(choice) else 1
            else:
                print(f"无效的选择: {choice}")
                print("请输入有效的模型名称或 'all' 或 'list'")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n用户取消操作")
        sys.exit(1)
    except Exception as e:
        print(f"\n发生错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

