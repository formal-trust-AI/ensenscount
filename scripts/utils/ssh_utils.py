import paramiko
import getpass
import os
import time


class SSHConnection:
    """SSH connection utility class with support for jump servers"""
    
    def __init__(self, user, server, host=None):
        """
        Initialize SSH connection parameters
        
        Args:
            user: Username for SSH connection
            server: Jump server hostname/IP (or direct host if host is None)
            host: Target host hostname/IP (optional, for jump server setup)
        """
        self.user = user
        self.server = server
        self.host = host if host else server
        self.use_jump_server = (server != self.host)
        
        self.ssh = None
        self.jump_ssh = None
        self._connected = False
    
    def _find_ssh_key(self):
        """Find available SSH private key"""
        key_paths = [
            os.path.expanduser('~/.ssh/id_ed25519'),
            os.path.expanduser('~/.ssh/id_rsa'),
            os.path.expanduser('~/.ssh/id_ecdsa')
        ]
        
        for key_path in key_paths:
            if os.path.exists(key_path):
                return key_path
        return None
    
    def connect(self):
        """Establish SSH connection"""
        if self._connected:
            return True
        
        valid_key_path = self._find_ssh_key()
        
        try:
            if not self.use_jump_server:
                # Direct connection
                self._connect_direct(valid_key_path)
            else:
                # Connection through jump server
                self._connect_through_jump_server(valid_key_path)
            
            self._connected = True
            return True
            
        except Exception as e:
            print(f"SSH connection failed: {e}")
            self.close()
            raise
    
    def _connect_direct(self, key_path):
        """Direct SSH connection"""
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        
        if key_path:
            self.ssh.connect(self.host, username=self.user, key_filename=key_path, 
                           allow_agent=True, look_for_keys=True, timeout=30)
        else:
            password = getpass.getpass(f'Password for {self.user}@{self.host}: ')
            self.ssh.connect(self.host, username=self.user, password=password, timeout=30)
    
    def _connect_through_jump_server(self, key_path):
        """Connect through jump server"""
        # Connect to jump server first
        self.jump_ssh = paramiko.SSHClient()
        self.jump_ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        
        if key_path:
            self.jump_ssh.connect(self.server, username=self.user, key_filename=key_path, 
                                allow_agent=True, look_for_keys=True, timeout=30)
        else:
            password = getpass.getpass(f'Password for {self.user}@{self.server}: ')
            self.jump_ssh.connect(self.server, username=self.user, password=password, timeout=30)
        
        # Create tunnel through jump server
        transport = self.jump_ssh.get_transport()
        dest_addr = (self.host, 22)
        local_addr = ('127.0.0.1', 22)
        channel = transport.open_channel("direct-tcpip", dest_addr, local_addr)
        
        # Give the channel a moment to establish
        time.sleep(0.5)
        
        # Connect to final host through the tunnel
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        
        if key_path:
            self.ssh.connect(self.host, username=self.user, key_filename=key_path, 
                           sock=channel, allow_agent=True, look_for_keys=True, timeout=30)
        else:
            password = getpass.getpass(f'Password for {self.user}@{self.host}: ')
            self.ssh.connect(self.host, username=self.user, password=password, sock=channel, timeout=30)
    
    def execute_command(self, cmd, cwd=None):
        """
        Execute a command on the remote server
        
        Args:
            cmd: Command to execute
            cwd: Working directory (optional)
            
        Returns:
            tuple: (stdout_output, stderr_output)
        """
        if not self._connected:
            raise RuntimeError("SSH connection not established. Call connect() first.")
        
        if cwd:
            cmd = f"cd {cwd} && {cmd}"
        
        print(f"Executing: {cmd}")
        stdin, stdout, stderr = self.ssh.exec_command(cmd)
        
        # Get the output and error streams
        stdout_output = stdout.read().decode()
        stderr_output = stderr.read().decode()
        exit_status = stdout.channel.recv_exit_status()
        
        if stdout_output:
            print(stdout_output)
        if stderr_output:
            print(f"STDERR: {stderr_output}")
        
        if exit_status != 0:
            raise RuntimeError(f"Command failed with exit status {exit_status}: {cmd}")
        
        return stdout_output, stderr_output
    
    def upload_files(self, local_dir, remote_dir):
        """
        Upload files to remote server using SCP
        
        Args:
            local_dir: Local directory path
            remote_dir: Remote directory path
        """
        if not self._connected:
            raise RuntimeError("SSH connection not established. Call connect() first.")
        
        from scp import SCPClient
        with SCPClient(self.ssh.get_transport()) as scp:
            # Upload all files and folders in local_dir, not the directory itself
            for item in os.listdir(local_dir):
                item_path = os.path.join(local_dir, item)
                scp.put(item_path, remote_path=remote_dir, recursive=True)
    
    def upload_selective_files(self, local_dir, remote_dir, include_patterns=None):
        """
        Upload only specific files/directories to remote server using SCP
        
        Args:
            local_dir: Local directory path
            remote_dir: Remote directory path
            include_patterns: List of file/directory patterns to include
        """
        if not self._connected:
            raise RuntimeError("SSH connection not established. Call connect() first.")
        
        from scp import SCPClient
        import fnmatch
        
        if include_patterns is None:
            # Default patterns for xgboost-counting project
            include_patterns = [
                'src',
                'include', 
                'lib.tar',
                'lib',
                'data',
                'config',
                'CMakeLists.txt',
                'Makefile',
                'scripts',
                '*.cpp',
                '*.hpp',
                '*.h',
                'remote_runner.sh'

            ]
        
        with SCPClient(self.ssh.get_transport()) as scp:
            for item in os.listdir(local_dir):
                item_path = os.path.join(local_dir, item)
                
                # Check if item matches any include pattern
                should_include = False
                for pattern in include_patterns:
                    if fnmatch.fnmatch(item, pattern):
                        should_include = True
                        break
                
                if should_include:
                    print(f"  Uploading: {item}")
                    scp.put(item_path, remote_path=remote_dir, recursive=True)
                else:
                    print(f"  Skipping: {item}")
    
    def download_files(self, remote_path, local_dir, recursive=False):
        """
        Download files from remote server using SCP
        
        Args:
            remote_path: Remote file/directory path
            local_dir: Local directory to save files
            recursive: Whether to download recursively
        """
        if not self._connected:
            raise RuntimeError("SSH connection not established. Call connect() first.")
        
        from scp import SCPClient
        os.makedirs(local_dir, exist_ok=True)
        
        with SCPClient(self.ssh.get_transport()) as scp:
            try:
                scp.get(remote_path, local_dir, recursive=recursive)
                return True
            except Exception as e:
                print(f"Download failed for {remote_path}: {e}")
                return False
    
    def is_connected(self):
        """Check if SSH connection is active"""
        return self._connected and self.ssh and self.ssh.get_transport() and self.ssh.get_transport().is_active()
    
    def close(self):
        """Close SSH connections"""
        if self.ssh:
            self.ssh.close()
            self.ssh = None
        
        if self.jump_ssh:
            self.jump_ssh.close()
            self.jump_ssh = None
        
        self._connected = False
    
    def __enter__(self):
        """Context manager entry"""
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.close()
