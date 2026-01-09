import { useEffect } from "react";
import { Form, Input, Button, message } from "antd";
import { FaUser, FaLock } from "react-icons/fa";
import { useNavigate } from "react-router-dom";
import "./index.scss";

interface LoginFormValues {
  username: string;
  password: string;
}

function Login() {
  const navigate = useNavigate();

  // 如果已登录，重定向到首页
  useEffect(() => {
    navigate("/channel", { replace: true });
  }, [navigate]);

  return (
    <div className="login-container">
      <div className="login-form-wrapper">
        <div className="login-form-container">
          <div className="login-title">系统登录</div>
          <Form
            name="login"
            autoComplete="off"
            size="large"
          >
            <Form.Item
              name="username"
              rules={[
                { required: true, message: "请输入用户名" },
                { min: 3, message: "用户名至少3个字符" },
              ]}
            >
              <Input
                prefix={<FaUser />}
                placeholder="用户名"
                autoComplete="username"
              />
            </Form.Item>

            <Form.Item
              name="password"
              rules={[
                { required: true, message: "请输入密码" },
                { min: 6, message: "密码至少6个字符" },
              ]}
            >
              <Input.Password
                prefix={<FaLock />}
                placeholder="密码"
                autoComplete="current-password"
              />
            </Form.Item>

            <Form.Item>
              <Button
                type="primary"
                htmlType="submit"
                block
              >
                登录
              </Button>
            </Form.Item>
          </Form>
        </div>
      </div>
    </div>
  );
}

export default Login;

