export interface MetaProps {
  keepAlive?: boolean;
  noRequiresAuth?: boolean;
  rank?: number;
}

export interface RouteObject {
  caseSensitive?: boolean;
  children?: RouteObject[];
  element?: React.ReactNode;
  index?: any;
  path?: string;
  meta?: MetaProps;
  icon?: string;
}
