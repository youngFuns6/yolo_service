import type { ProSettings } from "@ant-design/pro-components";
import { PageContainer, ProLayout } from "@ant-design/pro-components";
import { useEffect, useRef, useState } from "react";
import { Outlet, useLocation, useNavigate } from "react-router-dom";
import { rootRouter } from "@/routers/index";
import { searchRoute } from "@/routers/utils/router";

export interface OutletContextData {
  mainHeight: number;
}

export default () => {
  const [settings] = useState<Partial<ProSettings> | undefined>({
    fixSiderbar: true,
    layout: "side",
    splitMenus: false,
  });

  const navigate = useNavigate();
  const { pathname } = useLocation();
  const [contentHeight, setContentHeight] = useState(0);

  const mainContentRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const updateHeight = () => {
      if (mainContentRef.current) {
        setContentHeight(mainContentRef.current.offsetHeight);
      }
    };

    updateHeight();

    const resizeObserver = new ResizeObserver(updateHeight);
    if (mainContentRef.current) {
      resizeObserver.observe(mainContentRef.current);
    }

    return () => {
      resizeObserver.disconnect();
    };
  }, []);

  if (typeof document === "undefined") {
    return <div />;
  }

  return (
    <div
      id="test-pro-layout"
      style={{
        height: "100vh",
        overflow: "auto",
        position: "relative",
      }}
    >
      <ProLayout
        contentWidth="Fluid"
        title={""}
        logo="/logo.jpg"
        breadcrumbRender={false}
        route={{ path: "/", routes: rootRouter }}
        location={{
          pathname,
        }}
        token={{
          header: {
            colorBgMenuItemSelected: "rgba(0,0,0,0.04)",
          },
          sider: {
            colorMenuBackground: "#f5f5f5",
            colorTextMenuTitle: "rgba(0, 0, 0, 0.85)",
            colorTextMenu: "rgba(0, 0, 0, 0.65)",
            colorTextMenuSecondary: "rgba(0, 0, 0, 0.85)",
            colorTextMenuSelected: "#1890ff",
            colorTextMenuActive: "#1890ff",
            colorTextMenuItemHover: "#1890ff",
            colorBgMenuItemHover: "rgba(24, 144, 255, 0.1)",
            colorBgMenuItemSelected: "rgba(24, 144, 255, 0.15)",
          },
        }}
        menu={{
          collapsedShowGroupTitle: true,
          autoClose: false,
        }}
        menuDataRender={(menuData) => {
          // console.log(menuData);

          return menuData.map((item: any) => {
            if (item.children?.length === 1) {
              delete item.children;
              return item;
            } else {
              return item;
            }
          });
          // return menuData
        }}
        menuItemRender={(item, dom) => (
          <div
            onClick={() => {
              const current = searchRoute(item.path as string, rootRouter);
              if (!current) return;
              if (current.children) {
                // 如果第一个子路由是 index 路由，导航到父路由的 path
                const firstChild = current.children[0];
                if (firstChild?.index) {
                  navigate(item.path || "/");
                } else {
                  navigate(firstChild?.path || "/");
                }
              } else {
                navigate(item.path || "/");
              }
            }}
          >
            {dom}
          </div>
        )}
        {...settings}
      >
        <PageContainer
          content={
            <div
              ref={mainContentRef}
              style={{
                height: "100%",
                overflow: "auto",
              }}
            >
              <Outlet
                context={{
                  mainHeight: contentHeight,
                }}
              />
            </div>
          }
        ></PageContainer>
        {/* <SettingDrawer
          pathname={pathname}
          enableDarkTheme
          getContainer={(e: any) => {
            if (typeof window === "undefined") return e;
            return document.getElementById("test-pro-layout");
          }}
          settings={settings}
          onSettingChange={(changeSetting) => {
            setSetting(changeSetting);
          }}
          disableUrlParams={false}
        /> */}
      </ProLayout>
    </div>
  );
};
