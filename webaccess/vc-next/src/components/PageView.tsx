import type { PageData } from '../lib/types';
import { WidgetRenderer } from './WidgetRenderer';

interface Props {
  page: PageData;
}

export function PageView({ page }: Props) {
  return (
    <div
      className="page-root"
      style={{ position: 'relative', width: '100%', height: '100%' }}
    >
      {(page.children ?? []).map(w => (
        <WidgetRenderer key={w.id} widget={w} />
      ))}
    </div>
  );
}
