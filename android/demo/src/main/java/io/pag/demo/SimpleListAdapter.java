package io.pag.demo;

import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class SimpleListAdapter extends RecyclerView.Adapter<SimpleListAdapter.ItemHolder> {

    private final ItemClickListener itemClickListener;
    private final Object[] items;

    public SimpleListAdapter(Object[] items, ItemClickListener itemClickListener) {
        this.items = items;
        this.itemClickListener = itemClickListener;
    }

    @NonNull
    @Override
    public SimpleListAdapter.ItemHolder onCreateViewHolder(@NonNull ViewGroup viewGroup, int i) {
        return new ItemHolder(View.inflate(viewGroup.getContext(), R.layout.layout_item, null));
    }

    @Override
    public void onBindViewHolder(@NonNull final SimpleListAdapter.ItemHolder viewHolder, int position) {
        viewHolder.title.setText(items[position].toString());
        viewHolder.itemView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (itemClickListener != null) {
                    itemClickListener.onItemClick(viewHolder.getAdapterPosition());
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return items.length;
    }

    public interface ItemClickListener {
        void onItemClick(int position);
    }

    public static class ItemHolder extends RecyclerView.ViewHolder {

        TextView title;

        public ItemHolder(@NonNull View itemView) {
            super(itemView);
            title = itemView.findViewById(R.id.tv_title);
        }
    }
}
