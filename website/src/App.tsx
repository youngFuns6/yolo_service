import { ProConfigProvider } from "@ant-design/pro-components";
import { ConfigProvider, App as AntApp } from "antd";
import Router from "@/routers/index";

const App = () => {
  if (typeof document === "undefined") {
    return <div />;
  }

  return (
    <div
      id="test-pro-layout"
      style={{
        width: "100vw",
        height: "100vh",
        overflow: "auto",
      }}
    >
      <ProConfigProvider hashed={false}>
        <ConfigProvider>
          <AntApp style={{ height: "100%" }}>
            <Router />
          </AntApp>
        </ConfigProvider>
      </ProConfigProvider>
    </div>
  );
};

export default App;
